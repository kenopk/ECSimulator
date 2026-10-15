#include "InfiniteGraphicsView.h"
#include "Wire.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QDebug>
#include <cmath>
#include <QMenu>
#include <QContextMenuEvent>
#include <QUndoStack>
#include <QUndoCommand>
#include <QShortcut>
#include <QInputDialog>
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QVariantMap>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <functional>
#include <QClipboard>
#include <QMimeData>
#include <QApplication>

namespace {
constexpr int kEndpointPixelTolerance = 12;
constexpr int kBodyPixelTolerance  = 14;

//constexpr qreal kBodyLength    = 40.0;
//constexpr qreal kMinLeadLength = 10.0;
//constexpr qreal kMinLength     = kBodyLength + 2.0 * kMinLeadLength;
constexpr qreal kMinLength = 30.0;
QPoint ensureMinComponentLength(ComponentType type,
                                 const QPoint &fixedPos,
                                 const QPoint &movePos)
{
    qreal minLen = 0.0;
    switch (type) {
    case ComponentType::Resistor:
        minLen = kMinLength;
        break;
    case ComponentType::Non_Polar_Capacitor:
        minLen = kMinLength;
        break;
    case ComponentType::Polar_Capacitor: // <-- ДОБАВИТЬ
        minLen = kMinLength;
        break;
    default:
        return movePos;
    }

    QPoint d = movePos - fixedPos;
    qreal len = std::hypot(d.x(), d.y());

    if (len < 1e-6)
        return InfiniteGraphicsView::snapToGrid(QPointF(fixedPos.x() + minLen, fixedPos.y()));

    if (len < minLen) {
        QPointF result = QPointF(fixedPos) + QPointF(d) * (minLen / len);
        return InfiniteGraphicsView::snapToGrid(result);
    }

    return InfiniteGraphicsView::snapToGrid(QPointF(movePos));
}

// Разбор строки вида "120k", "1.2M", "470", "4,7k", "10R"
bool parseResistance(const QString &input, double &outOhms)
{
    QString s = input.trimmed();
    if (s.isEmpty()) return false;
    s.replace(',', '.');   // разрешаем запятую как десятичный разделитель
    s.remove(' ');              // обычный пробел
    s.remove(QChar(0x00A0));    // неразрывный пробел (пришёл копипастом из Word/Excel/браузера)
    s.remove(QChar(0x202F));    // узкий неразрывный пробел
    s.remove('\'');             // швейцарский разделитель: 1'000

    double mult = 1.0;
    const QChar last = s.at(s.size() - 1);
    if      (last == 'k' || last == 'K') { mult = 1e3;  s.chop(1); }
    else if (last == 'M')                { mult = 1e6;  s.chop(1); }
    else if (last == 'G' || last == 'g') { mult = 1e9;  s.chop(1); }
    else if (last == 'm')                { mult = 1e-3; s.chop(1); }
        else if (last == 'R' || last == 'r' || last == QChar(0x03A9)) { mult = 1.0; s.chop(1); }

    s = s.trimmed();
    if (s.isEmpty()) return false;

    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok || v < 0.0) return false;

    outOhms = v * mult;
    return outOhms <= 1e9;   // тот же потолок, что был у спинобокса
}

// Короткая форма: 120 000 -> "120k", 1500 -> "1.5k", 470 -> "470"
QString formatResistanceShort(double ohms)
{
    if (ohms >= 1e6) return QString::number(ohms / 1e6, 'g', 4) + 'M';
    if (ohms >= 1e3) return QString::number(ohms / 1e3, 'g', 4) + 'k';
    return QString::number(ohms, 'g', 4);
}

// Подсветка ошибки валидации
void setFieldError(QWidget *w, bool on)
{
    w->setProperty("error", on);
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

// Разбор ёмкости: "10uF", "100n", "0,1u", "470p", "1µF"
bool parseCapacitance(const QString &input, double &outF)
{
    QString s = input.trimmed();
    if (s.isEmpty()) return false;
    s.replace(',', '.');
    s.remove(' ');
    if (s.endsWith('F') || s.endsWith('f')) s.chop(1);
    if (s.isEmpty()) return false;

    double mult = 1.0;
    const QChar last = s.at(s.size() - 1);
    if      (last == 'p') { mult = 1e-12; s.chop(1); }
    else if (last == 'n') { mult = 1e-9;  s.chop(1); }
    else if (last == 'u' || last == QChar(0x00B5) || last == QChar(0x03BC)) { mult = 1e-6; s.chop(1); }
    else if (last == 'm') { mult = 1e-3;  s.chop(1); }
    else if (last == 'k') { mult = 1e3;   s.chop(1); }
    else if (last == 'M') { mult = 1e6;   s.chop(1); }

    s = s.trimmed();
    if (s.isEmpty()) return false;
    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok || v < 0.0) return false;
    outF = v * mult;
    return true;
}

QString formatCapacitanceShort(double f)
{
    if (f >= 1.0)  return QString::number(f, 'g', 4) + " F";
    if (f >= 1e-3) return QString::number(f * 1e3, 'g', 4) + " mF";
    if (f >= 1e-6) return QString::number(f * 1e6, 'g', 4) + " µF";
    if (f >= 1e-9) return QString::number(f * 1e9, 'g', 4) + " nF";
    return QString::number(f * 1e12, 'g', 4) + " pF";
}

// Перетаскивание диалога за шапку
class DragFilter : public QObject {
public:
    explicit DragFilter(QWidget *target) : QObject(target), m_target(target) {}
protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragOffset = me->globalPosition().toPoint() - m_target->frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                m_target->move(me->globalPosition().toPoint() - m_dragOffset);
                return true;
            }
        }
        return false;
    }
private:
    QWidget *m_target = nullptr;
    QPoint m_dragOffset;
};

// Enter в спинбоксе = OK
class EnterKeyFilter : public QObject {
public:
    EnterKeyFilter(QWidget *target, std::function<void()> handler)
        : QObject(target), m_handler(std::move(handler)) {}
protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                m_handler();
                return true;
            }
        }
        return false;
    }
private:
    std::function<void()> m_handler;
};

// Описание одного поля диалога
struct ParamField {
    QString label;
    double  value    = 0.0;
    double  min      = 0.0;
    double  max      = 1e9;
    int     decimals = 3;
    double  step     = 1.0;
    enum Kind { Spin, ResText, CapText } kind = Spin;
};

const char *kDlgContentStyle = R"(
#dlgContent { background-color:#161b22; border-bottom-left-radius:8px; border-bottom-right-radius:8px; }
#dlgContent QLabel { color:#e6edf3; font-size:13px; }
#dlgContent QLineEdit, #dlgContent QDoubleSpinBox {
    background-color:#0d1117; color:#ffffff; border:1px solid #30363d;
    border-radius:4px; padding:4px 6px; font-size:13px; min-height:22px; }
#dlgContent QLineEdit:focus, #dlgContent QDoubleSpinBox:focus { border:1px solid #58a6ff; }
#dlgContent QLineEdit[error="true"], #dlgContent QDoubleSpinBox[error="true"] { border:1px solid #ff4444; }
#dlgContent QDoubleSpinBox::up-button, #dlgContent QDoubleSpinBox::down-button { background-color:#21262d; border:none; width:16px; }
#dlgContent QDoubleSpinBox::up-button:hover, #dlgContent QDoubleSpinBox::down-button:hover { background-color:#30363d; }
)";

const char *kGreenBtnStyle = R"(
QPushButton { background-color:#238636; color:white; border:none; border-radius:6px; padding:6px 0px; font-weight:bold; font-size:13px; }
QPushButton:hover { background-color:#2ea043; })";

const char *kGreyBtnStyle = R"(
QPushButton { background-color:#21262d; color:#c9d1d9; border:none; border-radius:6px; padding:6px 0px; font-weight:bold; font-size:13px; }
QPushButton:hover { background-color:#30363d; })";

// =========================================================
// УНИВЕРСАЛЬНОЕ ОКНО ПАРАМЕТРОВ: строит диалог по списку полей,
// applyFn вызывается при «Применить» и при OK
// =========================================================
void showComponentParamsDialog(
    QWidget *parent,
    const QString &title,
    const QVector<ParamField> &fields,
    const std::function<void(const QVector<double>&)> &applyFn)
{
    QDialog dialog(parent);
    dialog.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dialog.setAttribute(Qt::WA_TranslucentBackground);

    auto *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---------- шапка ----------
    auto *headerWidget = new QWidget();
    headerWidget->setObjectName("dlgHeader");
    headerWidget->setStyleSheet(R"(
        #dlgHeader { background-color:#0d1117;
                     border-top-left-radius:8px; border-top-right-radius:8px; })");
    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(15, 8, 8, 8);
    headerLayout->setSpacing(10);
    auto *headerTitle = new QLabel(title);
    headerTitle->setStyleSheet("color:#e6edf3; font-size:14px; font-weight:bold;");
    auto *closeBtn = new QPushButton("✕");
    closeBtn->setFixedSize(30, 30);
    closeBtn->setStyleSheet(R"(
        QPushButton { background-color:transparent; color:#8b949e; border:none; font-size:18px; font-weight:bold; }
        QPushButton:hover { background-color:#ff4444; color:white; border-radius:4px; })");
    QObject::connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(closeBtn);
    headerWidget->installEventFilter(new DragFilter(&dialog));
    mainLayout->addWidget(headerWidget);

    // ---------- контент ----------
    auto *contentWidget = new QWidget();
    contentWidget->setObjectName("dlgContent");
    contentWidget->setStyleSheet(kDlgContentStyle);
    auto *form = new QFormLayout(contentWidget);
    form->setContentsMargins(15, 10, 15, 15);
    form->setVerticalSpacing(10);
    form->setHorizontalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    QVector<QLineEdit*>      edits(fields.size(), nullptr);
    QVector<QDoubleSpinBox*> spins(fields.size(), nullptr);

    for (int i = 0; i < fields.size(); ++i) {
        const ParamField &f = fields[i];
        QWidget *fieldWidget = nullptr;
        if (f.kind == ParamField::Spin) {
            auto *spin = new QDoubleSpinBox();
            spin->setRange(f.min, f.max);
            spin->setDecimals(f.decimals);
            spin->setSingleStep(f.step);
            spin->setValue(f.value);
            spins[i] = spin;
            fieldWidget = spin;
        } else {
            auto *edit = new QLineEdit();
            edit->setText(f.kind == ParamField::ResText
                              ? formatResistanceShort(f.value)
                              : formatCapacitanceShort(f.value));
            edit->setPlaceholderText(f.kind == ParamField::ResText
                                         ? "Напр.: 120k, 1.2M, 470"
                                         : "Напр.: 10µF, 100n, 0.1u");
            QPalette pal = edit->palette();
            pal.setColor(QPalette::PlaceholderText, QColor(0x8b, 0x94, 0x9e));
            edit->setPalette(pal);
            edits[i] = edit;
            fieldWidget = edit;
        }
        fieldWidget->setFixedWidth(100);
        form->addRow(f.label, fieldWidget);
    }

    // ---------- чтение + валидация ----------
    auto readValues = [&](QVector<double> &vals) -> bool {
        vals.resize(fields.size());
        for (int i = 0; i < fields.size(); ++i) {
            if (edits[i]) {
                double v = 0.0;
                const bool ok = (fields[i].kind == ParamField::ResText)
                                    ? parseResistance(edits[i]->text(), v)
                                    : parseCapacitance(edits[i]->text(), v);
                if (!ok) {
                    setFieldError(edits[i], true);
                    edits[i]->setFocus();
                    edits[i]->selectAll();
                    return false;
                }
                setFieldError(edits[i], false);
                vals[i] = v;
            } else {
                vals[i] = spins[i]->value();
            }
        }
        return true;
    };

    auto applyChanges = [&]() {
        QVector<double> vals;
        if (readValues(vals)) applyFn(vals);
    };
    auto okHandler = [&]() {
        QVector<double> vals;
        if (readValues(vals)) {
            applyFn(vals);
            dialog.accept();
        }
    };

    // ---------- кнопки ----------
    auto *buttonWidget = new QWidget();
    auto *btnLayout = new QHBoxLayout(buttonWidget);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(8);

    auto *applyBtn  = new QPushButton("Применить");
    auto *okBtn     = new QPushButton("OK");
    auto *cancelBtn = new QPushButton("Отмена");
    applyBtn->setStyleSheet(kGreenBtnStyle);
    okBtn->setStyleSheet(kGreenBtnStyle);
    cancelBtn->setStyleSheet(kGreyBtnStyle);
    for (auto *b : {applyBtn, okBtn, cancelBtn})
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QObject::connect(applyBtn,  &QPushButton::clicked, applyChanges);
    QObject::connect(okBtn,     &QPushButton::clicked, okHandler);
    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    okBtn->setDefault(true);
    okBtn->setAutoDefault(true);
    applyBtn->setAutoDefault(false);
    cancelBtn->setAutoDefault(false);

    for (int i = 0; i < fields.size(); ++i) {
        if (edits[i]) {
            QObject::connect(edits[i], &QLineEdit::editingFinished, [&, i]() {
                double v = 0.0;
                const bool ok = (fields[i].kind == ParamField::ResText)
                                    ? parseResistance(edits[i]->text(), v)
                                    : parseCapacitance(edits[i]->text(), v);
                if (ok) {
                    edits[i]->setText(fields[i].kind == ParamField::ResText
                                          ? formatResistanceShort(v)
                                          : formatCapacitanceShort(v));
                    setFieldError(edits[i], false);
                }
            });
            QObject::connect(edits[i], &QLineEdit::returnPressed, okHandler);
        }
        if (spins[i])
            spins[i]->installEventFilter(new EnterKeyFilter(spins[i], okHandler));
    }

    btnLayout->addWidget(applyBtn, 1);
    btnLayout->addWidget(okBtn, 1);
    btnLayout->addWidget(cancelBtn, 1);
    form->addRow(buttonWidget);          // кнопки ВНУТРИ контента — фон накрывает их
    mainLayout->addWidget(contentWidget);

    dialog.exec();
}

constexpr char kClipboardMime[] = "application/x-ecsimulator-wire-group";

QByteArray serializeClipboard(const QList<InfiniteGraphicsView::CopiedWireData> &items)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << qint32(items.size());
    for (const auto &d : items) {
        out << d.relativeStart << d.relativeEnd
            << d.startConnected << d.endConnected
            << d.isComponent << qint32(d.componentType)
            << d.params;
    }
    return bytes;
}

QList<InfiniteGraphicsView::CopiedWireData> deserializeClipboard(const QByteArray &bytes)
{
    QList<InfiniteGraphicsView::CopiedWireData> items;
    QDataStream in(bytes);
    in.setVersion(QDataStream::Qt_6_0);
    qint32 count = 0;
    in >> count;
    for (int i = 0; i < count; ++i) {
        InfiniteGraphicsView::CopiedWireData d;
        qint32 typeInt = 0;
        in >> d.relativeStart >> d.relativeEnd
            >> d.startConnected >> d.endConnected
            >> d.isComponent >> typeInt >> d.params;
        d.componentType = static_cast<ComponentType>(typeInt);
        items.append(d);
    }
    return items;
}
} // anonymous namespace

InfiniteGraphicsView::InfiniteGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_pasteOffset = QPoint(0, 0);
    m_lastPastePos = QPoint(0, 0);
    m_hasPasted = false;
    m_dragWithShift = false;
    m_hoveredWire = nullptr;
    m_hoveredEndpointIsStart = false;
    setAttribute(Qt::WA_NoMouseReplay);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);
    m_undoStack = new QUndoStack(this);

    setDragMode(QGraphicsView::RubberBandDrag);
    QPalette pal = palette();
    pal.setColor(QPalette::Highlight, QColor(255, 200, 0, 60));
    setPalette(pal);
    setBackgroundBrush(QColor(30, 30, 46));
    viewport()->setAutoFillBackground(true);
    setCursor(Qt::OpenHandCursor);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::StrongFocus);

    m_wireShortcut = new QShortcut(QKeySequence(Qt::Key_W), this);
    m_wireShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(m_wireShortcut, &QShortcut::activated, this, [this]() {
        setWireMode(!isWireMode());
    });

    m_resistorShortcut = new QShortcut(QKeySequence(Qt::Key_R), this);
    m_resistorShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(m_resistorShortcut, &QShortcut::activated, this, [this]() {
        if (isComponentMode() && currentComponentType() == ComponentType::Resistor)
            disableComponentMode();
        else
            setComponentMode(ComponentType::Resistor);
    });

    m_capacitorShortcut = new QShortcut(QKeySequence(Qt::Key_C), this);
    m_capacitorShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(m_capacitorShortcut, &QShortcut::activated, this, [this]() {
        if (isComponentMode() && currentComponentType() == ComponentType::Non_Polar_Capacitor)
            disableComponentMode();
        else
            setComponentMode(ComponentType::Non_Polar_Capacitor);
    });

    m_polarCapacitorShortcut = new QShortcut(QKeySequence(Qt::Key_E), this); // E от Electrolytic
    m_polarCapacitorShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(m_polarCapacitorShortcut, &QShortcut::activated, this, [this]() {
        if (isComponentMode() && currentComponentType() == ComponentType::Polar_Capacitor)
            disableComponentMode();
        else
            setComponentMode(ComponentType::Polar_Capacitor);
    });

    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setRenderHint(QPainter::Antialiasing, false);
    setRenderHint(QPainter::SmoothPixmapTransform, false);
    qDebug() << "InfiniteGraphicsView created (Optimized & Center Zoom)";
}

void InfiniteGraphicsView::setWireMode(bool on) {
    qDebug() << "setWireMode" << on << "from" << Q_FUNC_INFO;
    if (on) disableComponentMode();
    m_wireMode = on;
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

QPoint InfiniteGraphicsView::snapToGrid(const QPointF &pos, qreal gridSize) {
    int x = static_cast<int>(std::round(pos.x() / gridSize) * gridSize);
    int y = static_cast<int>(std::round(pos.y() / gridSize) * gridSize);
    return QPoint(x, y);
}

void InfiniteGraphicsView::wheelEvent(QWheelEvent *event) {
    qreal factor = 1.2;
    if (event->angleDelta().y() > 0)
        scale(factor, factor);
    else
        scale(1.0 / factor, 1.0 / factor);
    event->accept();
}

void InfiniteGraphicsView::clearHover() {
    if (m_hoveredWire) {
        if (scene() && scene()->items().contains(m_hoveredWire)) {
            m_hoveredWire->setHovered(false);
            m_hoveredWire->setEndpointHovered(true, false);
            m_hoveredWire->setEndpointHovered(false, false);
            m_hoveredWire->setZValue(0);
        }
        m_hoveredWire = nullptr;
        m_hoveredEndpointIsStart = false;
    }
}

void InfiniteGraphicsView::collectNodeWires(const QPoint &anchor)
{
    m_connectedWiresAtPress.clear();
    if (!scene()) return;
    const qreal eps = 5.0;
    for (QGraphicsItem *it : scene()->items()) {
        Wire *w = dynamic_cast<Wire*>(it);
        if (!w) continue;
        if ((w->startScenePos().toPoint() - anchor).manhattanLength() < eps)
            m_connectedWiresAtPress.append({w, true});
        if ((w->endScenePos().toPoint() - anchor).manhattanLength() < eps)
            m_connectedWiresAtPress.append({w, false});
    }
}

namespace {
qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b) {
    QPointF ab = b - a;
    qreal lenSq = QPointF::dotProduct(ab, ab);
    if (lenSq < 0.0001) return QLineF(p, a).length();
    QPointF ap = p - a;
    qreal t = QPointF::dotProduct(ap, ab) / lenSq;
    t = qBound(0.0, t, 1.0);
    QPointF closest = a + t * ab;
    return QLineF(p, closest).length();
}

void recalculateAllWireConnections(QGraphicsScene *scene) {
    QList<Wire*> wires;
    const QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem *item : items) {
        if (Wire *w = dynamic_cast<Wire*>(item)) wires.append(w);
    }

    for (Wire *w : wires) {
        w->setStartConnected(false);
        w->setEndConnected(false);
        w->setStartNearOther(false);
        w->setEndNearOther(false);
    }

    const qreal proximityRadius = 10.0;

    for (int i = 0; i < wires.size(); ++i) {
        for (int j = i + 1; j < wires.size(); ++j) {
            Wire *a = wires[i];
            Wire *b = wires[j];

            QPointF aStart = a->startScenePos();
            QPointF aEnd   = a->endScenePos();
            QPointF bStart = b->startScenePos();
            QPointF bEnd   = b->endScenePos();

            const qreal eps = 3.0;

            if (QLineF(aStart, bStart).length() < eps) {
                QPointF mid = (aStart + bStart) / 2.0;
                a->setStartPos(mid.toPoint());
                b->setStartPos(mid.toPoint());
                a->setStartConnected(true); b->setStartConnected(true);
            }
            if (QLineF(aStart, bEnd).length() < eps) {
                QPointF mid = (aStart + bEnd) / 2.0;
                a->setStartPos(mid.toPoint());
                b->setEndPos(mid.toPoint());
                a->setStartConnected(true); b->setEndConnected(true);
            }
            if (QLineF(aEnd, bStart).length() < eps) {
                QPointF mid = (aEnd + bStart) / 2.0;
                a->setEndPos(mid.toPoint());
                b->setStartPos(mid.toPoint());
                a->setEndConnected(true);   b->setStartConnected(true);
            }
            if (QLineF(aEnd, bEnd).length() < eps) {
                QPointF mid = (aEnd + bEnd) / 2.0;
                a->setEndPos(mid.toPoint());
                b->setEndPos(mid.toPoint());
                a->setEndConnected(true);   b->setEndConnected(true);
            }

            auto checkPointToSegment = [&](const QPointF &p, Wire *w, bool isStart,
                                           const QPointF &segP1, const QPointF &segP2)
            {
                if ((isStart && w->isStartConnected()) || (!isStart && w->isEndConnected())) {
                    return;
                }
                qreal dist = distanceToSegment(p, segP1, segP2);
                if (dist < proximityRadius) {
                    if (isStart) w->setStartNearOther(true);
                    else         w->setEndNearOther(true);
                }
            };

            checkPointToSegment(aStart, a, true,  bStart, bEnd);
            checkPointToSegment(aEnd,   a, false, bStart, bEnd);
            checkPointToSegment(bStart, b, true,  aStart, aEnd);
            checkPointToSegment(bEnd,   b, false, aStart, aEnd);
        }
    }
}
} // namespace

// =========================================================
// Классы-команды для системы Undo/Redo
// =========================================================

namespace {

class AddComponentCommand : public QUndoCommand {
public:
    AddComponentCommand(QGraphicsScene *scene, Component *comp)
        : QUndoCommand("Добавление компонента"), m_scene(scene), m_component(comp) {}

    ~AddComponentCommand() override {
        if (!m_added) delete m_component;
    }

    void redo() override {
        m_scene->addItem(m_component);
        m_added = true;
        // recalculateAllWireConnections(m_scene);
    }

    void undo() override {
        m_scene->removeItem(m_component);
        m_added = false;
        // recalculateAllWireConnections(m_scene);
    }

private:
    QGraphicsScene *m_scene;
    Component *m_component;
    bool m_added = false;
};

class ChangeParamCommand : public QUndoCommand {
public:
    ChangeParamCommand(Component *comp, const QString &key,
                       const QVariant &oldVal, const QVariant &newVal)
        : QUndoCommand("Изменение параметра"), m_comp(comp), m_key(key),
        m_old(oldVal), m_new(newVal) {}

    void redo() override { m_comp->setParam(m_key, m_new); }
    void undo() override { m_comp->setParam(m_key, m_old); }

private:
    Component *m_comp;
    QString m_key;
    QVariant m_old, m_new;
};

struct ParamChange { QString key; double newValue; };

class ChangeParamCommand;

void pushParamChanges(QUndoStack *stack, Component *comp,
                      const QString &macroTitle,
                      const QVector<ParamChange> &changes)
{
    bool any = false;
    for (const auto &c : changes)
        if (qAbs(c.newValue - comp->param(c.key).toDouble()) > 1e-9) any = true;
    if (!any) return;

    stack->beginMacro(macroTitle);
    for (const auto &c : changes) {
        const double oldVal = comp->param(c.key).toDouble();
        stack->push(new ChangeParamCommand(comp, c.key, oldVal, c.newValue));
        comp->setParam(c.key, c.newValue);
    }
    stack->endMacro();
}

class MoveMultipleEndpointsCommand : public QUndoCommand {
public:
    struct Entry {
        Wire *wire;
        bool isStart;
        QPointF oldPos;
        QPointF newPos;
    };

    MoveMultipleEndpointsCommand(QGraphicsScene *scene, QVector<Entry> entries)
        : QUndoCommand("Перемещение концов узла"), m_scene(scene), m_entries(std::move(entries)) {}

    void redo() override {
        for (const Entry &e : m_entries) {
            if (m_scene && m_scene->items().contains(e.wire)) {
                if (e.isStart) e.wire->setStartPos(e.newPos.toPoint());
                else           e.wire->setEndPos(e.newPos.toPoint());
            }
        }
        if (m_scene) recalculateAllWireConnections(m_scene);
    }

    void undo() override {
        for (const Entry &e : m_entries) {
            if (m_scene && m_scene->items().contains(e.wire)) {
                if (e.isStart) e.wire->setStartPos(e.oldPos.toPoint());
                else           e.wire->setEndPos(e.oldPos.toPoint());
            }
        }
        if (m_scene) recalculateAllWireConnections(m_scene);
    }

private:
    QGraphicsScene *m_scene;
    QVector<Entry> m_entries;
};

class AddWireCommand : public QUndoCommand {
public:
    AddWireCommand(QGraphicsScene *scene, Wire *wire)
        : QUndoCommand("Добавление провода"), m_scene(scene), m_wire(wire) {}

    ~AddWireCommand() override {
        if (!m_added) delete m_wire;
    }

    void redo() override {
        m_scene->addItem(m_wire);
        m_added = true;
        // recalculateAllWireConnections(m_scene);
    }

    void undo() override {
        m_scene->removeItem(m_wire);
        m_added = false;
        // recalculateAllWireConnections(m_scene);
    }

private:
    QGraphicsScene *m_scene;
    Wire *m_wire;
    bool m_added = false;
};

class MoveWireEndpointCommand : public QUndoCommand {
public:
    MoveWireEndpointCommand(QGraphicsScene *scene, Wire *wire, bool isStart,
                            QPointF oldPos, QPointF newPos)
        : QUndoCommand("Перемещение конца провода"),
        m_scene(scene), m_wire(wire), m_isStart(isStart),
        m_oldPos(oldPos), m_newPos(newPos) {}

    void redo() override {
        if (m_isStart) m_wire->setStartPos(m_newPos.toPoint());
        else           m_wire->setEndPos(m_newPos.toPoint());
        recalculateAllWireConnections(m_scene);
    }

    void undo() override {
        qDebug() << "AddWireCommand::undo() called";
        if (m_isStart) m_wire->setStartPos(m_oldPos.toPoint());
        else           m_wire->setEndPos(m_oldPos.toPoint());
        recalculateAllWireConnections(m_scene);
    }

private:
    QGraphicsScene *m_scene;
    Wire *m_wire;
    bool m_isStart;
    QPointF m_oldPos, m_newPos;
};

class MoveItemsCommand : public QUndoCommand {
public:
    struct Entry {
        Wire *wire;
        QPointF oldStart, oldEnd;
        QPointF newStart, newEnd;
    };

    MoveItemsCommand(QGraphicsScene *scene, QVector<Entry> entries)
        : QUndoCommand("Перемещение элементов"),
        m_scene(scene), m_entries(std::move(entries)) {}

    void redo() override {
        for (const Entry &e : m_entries) {
            e.wire->setStartPos(e.newStart.toPoint());
            e.wire->setEndPos(e.newEnd.toPoint());
        }
        recalculateAllWireConnections(m_scene);
    }

    void undo() override {
        for (const Entry &e : m_entries) {
            e.wire->setStartPos(e.oldStart.toPoint());
            e.wire->setEndPos(e.oldEnd.toPoint());
        }
        recalculateAllWireConnections(m_scene);
    }

private:
    QGraphicsScene *m_scene;
    QVector<Entry> m_entries;
};

class RemoveItemsCommand : public QUndoCommand {
public:
    RemoveItemsCommand(QGraphicsScene *scene, const QList<QGraphicsItem*> &items)
        : QUndoCommand("Удаление элементов"), m_scene(scene), m_items(items) {}

    ~RemoveItemsCommand() override {
        if (m_removed) qDeleteAll(m_items);
    }

    void redo() override {
        for (QGraphicsItem *item : m_items) {
            item->setSelected(false);
            m_scene->removeItem(item);
        }
        m_removed = true;
        recalculateAllWireConnections(m_scene);
    }

    void undo() override {
        for (QGraphicsItem *item : m_items) m_scene->addItem(item);
        m_removed = false;
        recalculateAllWireConnections(m_scene);
    }

private:
    QGraphicsScene *m_scene;
    QList<QGraphicsItem*> m_items;
    bool m_removed = false;
};

} // namespace

// =========================================================
// Методы InfiniteGraphicsView
// =========================================================

void InfiniteGraphicsView::deleteSelectedItems() {
    clearHover();
    if (!scene()) return;
    const QList<QGraphicsItem*> items = scene()->selectedItems();
    if (items.isEmpty()) return;
    m_undoStack->push(new RemoveItemsCommand(scene(), items));
    viewport()->update();
}

void InfiniteGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    event->ignore();
    clearHover();
}

void InfiniteGraphicsView::mousePressEvent(QMouseEvent *event) {
    setFocus(Qt::MouseFocusReason);

    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        QGraphicsItem *itemUnderCursor = itemAt(event->pos());
        if (itemUnderCursor && (itemUnderCursor->flags() & QGraphicsItem::ItemIsSelectable)) {
            if (!itemUnderCursor->isSelected()) {

                itemUnderCursor->setSelected(true);
            }
            viewport()->update();
            event->accept();
            return;
        }
        QGraphicsView::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton && (m_wireMode || m_componentMode)) {
        if (!scene()) {
            qDebug() << "InfiniteGraphicsView::mousePressEvent() - scene is null!";
            event->accept();
            return;
        }
        QPoint startPos = snapToGrid(mapToScene(event->pos()));
        m_wireStartPin.owner = nullptr;
        m_wireStartPin.standalonePos = startPos;
        m_wireDrawing = true;

        if (m_previewLine) {
            scene()->removeItem(m_previewLine);
            delete m_previewLine;
            m_previewLine = nullptr;
        }
        m_previewLine = scene()->addLine(
            QLineF(startPos.x(), startPos.y(), startPos.x(), startPos.y()),
            QPen(Qt::yellow, 1, Qt::DashLine)
        );
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && !m_wireMode && !m_componentMode) {
        QPoint scenePos = mapToScene(event->pos()).toPoint();

        bool isStart = false;
        Wire *wire = findWireEndpointNear(scenePos, &isStart);
        if (wire) {
            if (wire->isSelected() && scene()->selectedItems().size() == 1) {
                m_draggedWire = wire;
                m_draggingStart = isStart;
                m_dragOldPos = isStart ? wire->startScenePos().toPoint() : wire->endScenePos().toPoint();
                m_dragWithShift = (event->modifiers() & Qt::ShiftModifier);
                if (m_dragWithShift) collectNodeWires(m_dragOldPos);
                else m_connectedWiresAtPress.clear();
                event->accept();
                return;
            }
            else if (wire->isSelected()) {
                m_movingWires.clear();
                const QList<QGraphicsItem*> selected = scene()->selectedItems();
                for (QGraphicsItem *it : selected) {
                    if (Wire *w = dynamic_cast<Wire*>(it))
                        m_movingWires.append(w);
                }
                if (m_movingWires.isEmpty() && wire->isSelected()) {
                    m_movingWires.append(wire);
                }
                m_moveOriginalPositions.clear();
                for (Wire *w : m_movingWires) {
                    m_moveOriginalPositions.insert(w, { w->startScenePos().toPoint(), w->endScenePos().toPoint() });
                }
                m_movePressScenePos = snapToGrid(mapToScene(event->pos()));
                event->accept();
                return;
            }
            else {
                m_draggedWire = wire;
                m_draggingStart = isStart;
                m_dragOldPos = isStart ? wire->startScenePos().toPoint() : wire->endScenePos().toPoint();
                m_dragWithShift = (event->modifiers() & Qt::ShiftModifier);
                if (m_dragWithShift) collectNodeWires(m_dragOldPos);
                else m_connectedWiresAtPress.clear();
                event->accept();
                return;
            }
        }

        Wire *wireUnderCursor = nullptr;
        const QList<QGraphicsItem*> items = scene()->items(scenePos, Qt::IntersectsItemShape,
                                                            Qt::DescendingOrder, transform());
        for (QGraphicsItem *it : items) {
            if (Wire *w = dynamic_cast<Wire*>(it)) {
                if (w->shape().contains(w->mapFromScene(scenePos))) {
                    wireUnderCursor = w;
                    break;
                }
            }
        }
        if (wireUnderCursor) {
            if (!wireUnderCursor->isSelected()) {
                wireUnderCursor->setSelected(true);
            }
            m_movingWires.clear();
            const QList<QGraphicsItem*> selected = scene()->selectedItems();
            for (QGraphicsItem *it : selected) {
                if (Wire *w = dynamic_cast<Wire*>(it))
                    m_movingWires.append(w);
            }
            m_moveOriginalPositions.clear();
            for (Wire *w : m_movingWires) {
                m_moveOriginalPositions.insert(w, { w->startScenePos().toPoint(), w->endScenePos().toPoint() });
            }
            m_movePressScenePos = snapToGrid(mapToScene(event->pos()));
            event->accept();
            return;
        }

        clearHover();
        QGraphicsView::mousePressEvent(event);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void InfiniteGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    bool isLeftButton = (event->button() == Qt::LeftButton);
    if (!scene()) {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    if (isLeftButton && !m_movingWires.isEmpty()) {
        QList<Wire*> aliveWires;
        for (Wire *w : m_movingWires) {
            if (scene()->items().contains(w))
                aliveWires.append(w);
        }
        if (!aliveWires.isEmpty()) {
            QPoint currentSnapped = snapToGrid(mapToScene(event->pos()));
            QPoint delta = currentSnapped - m_movePressScenePos;
            if (delta != QPoint(0, 0)) {
                QVector<MoveItemsCommand::Entry> entries;
                for (Wire *w : aliveWires) {
                    const auto &orig = m_moveOriginalPositions.value(w);
                    MoveItemsCommand::Entry e;
                    e.wire = w;
                    e.oldStart = QPointF(orig.first);
                    e.oldEnd = QPointF(orig.second);
                    e.newStart = QPointF(orig.first + delta);
                    e.newEnd = QPointF(orig.second + delta);
                    entries.append(e);
                }
                m_undoStack->push(new MoveItemsCommand(scene(), entries));
            } else {
                recalculateAllWireConnections(scene());
            }
        }
        m_movingWires.clear();
        m_moveOriginalPositions.clear();
    }

    if (isLeftButton && m_draggedWire) {
        if (scene()->items().contains(m_draggedWire)) {
            QPoint newPos = m_draggingStart ? m_draggedWire->startScenePos().toPoint()
                                             : m_draggedWire->endScenePos().toPoint();

            if ((newPos - m_dragOldPos).manhattanLength() > 0) {
                if (m_dragWithShift) {
                    QVector<MoveMultipleEndpointsCommand::Entry> entries;
                    for (const auto &entry : m_connectedWiresAtPress) {
                        Wire *w = entry.first;
                        if (!scene()->items().contains(w)) continue;
                        QPointF newP = entry.second ? w->startScenePos() : w->endScenePos();
                        entries.push_back(MoveMultipleEndpointsCommand::Entry{
                            w, entry.second, QPointF(m_dragOldPos), newP });
                    }
                    if (!entries.isEmpty()) {
                        m_undoStack->push(new MoveMultipleEndpointsCommand(scene(), entries));
                    } else {
                        recalculateAllWireConnections(scene());
                    }
                } else {
                    m_undoStack->push(new MoveWireEndpointCommand(
                        scene(), m_draggedWire, m_draggingStart, QPointF(m_dragOldPos), QPointF(newPos)));
                }
            } else {
                recalculateAllWireConnections(scene());
            }
        }
        m_draggedWire = nullptr;
        m_connectedWiresAtPress.clear();
    }

    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        setCursor(m_wireMode ? Qt::CrossCursor : Qt::OpenHandCursor);
        if (!m_movingWires.isEmpty()) {
            m_movePressScenePos = snapToGrid(mapToScene(event->pos()));
            m_moveOriginalPositions.clear();
            for (Wire *w : m_movingWires) {
                m_moveOriginalPositions.insert(w, { w->startScenePos().toPoint(), w->endScenePos().toPoint() });
            }
        }
        event->accept();
        return;
    }

    if (isLeftButton && m_wireDrawing) {
        QPoint endPos = snapToGrid(mapToScene(event->pos()));
        if (m_previewLine) {
            scene()->removeItem(m_previewLine);
            delete m_previewLine;
            m_previewLine = nullptr;
        }
        m_wireDrawing = false;

        qreal length = QLineF(m_wireStartPin.scenePos(), endPos).length();

        if (m_componentMode && length > 0.1) {
            endPos = ensureMinComponentLength(m_componentType,
                                              m_wireStartPin.standalonePos,
                                              endPos);
            length = QLineF(m_wireStartPin.scenePos(), endPos).length();
        }

        if (length > kGridSize) {
            Pin endPin;
            endPin.owner = nullptr;
            endPin.standalonePos = endPos;

            if (m_componentMode) {
                Component *comp = new Component(m_wireStartPin, endPin, m_componentType);
                m_undoStack->push(new AddComponentCommand(scene(), comp));
            } else if (m_wireMode) {
                Wire *wire = new Wire(m_wireStartPin, endPin);
                m_undoStack->push(new AddWireCommand(scene(), wire));
            }
            recalculateAllWireConnections(scene());
        }
    }

    if (isLeftButton) {
        setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
        viewport()->update();
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void InfiniteGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (m_panning) {
        QPoint currentPos = event->pos();
        QPoint delta = currentPos - m_lastPanPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_lastPanPos = currentPos;
        viewport()->update();
        event->accept();
        return;
    }

    if (!m_movingWires.isEmpty()) {
        QPoint currentSnapped = snapToGrid(mapToScene(event->pos()));
        QPoint delta = currentSnapped - m_movePressScenePos;
        for (Wire *w : m_movingWires) {
            const auto &orig = m_moveOriginalPositions.value(w);
            w->setStartPos(orig.first + delta);
            w->setEndPos(orig.second + delta);
        }
        viewport()->update();
        event->accept();
        return;
    }

    // 3. Перетаскивание одного конца провода
    if (m_draggedWire) {
        if (!scene() || !scene()->items().contains(m_draggedWire)) {
            m_draggedWire = nullptr;
            m_connectedWiresAtPress.clear();
            event->accept();
            return;
        }
        QPoint pos = snapToGrid(mapToScene(event->pos()));

        // Магнитный снап (отключается при Shift)
        if (!(event->modifiers() & Qt::ShiftModifier)) {
            const qreal snapRadius = 15.0;
            const QList<QGraphicsItem*> allItems = scene()->items();
            for (QGraphicsItem *it : allItems) {
                Wire *w = dynamic_cast<Wire*>(it);
                if (!w || w == m_draggedWire) continue;
                QPoint nearStart = w->startScenePos().toPoint();
                QPoint nearEnd = w->endScenePos().toPoint();
                if (QLineF(pos, nearStart).length() < snapRadius) { pos = nearStart; break; }
                if (QLineF(pos, nearEnd).length() < snapRadius)   { pos = nearEnd;   break; }
            }
        }

        const QPoint oppositePos = m_draggingStart ? m_draggedWire->endScenePos().toPoint()
                                                   : m_draggedWire->startScenePos().toPoint();
        if (pos == oppositePos) {
            // Пропускаем этот кадр движения — провод сохраняет длину.
            // (Для резистора это и так гарантирует ensureMinComponentLength,
            //  а вот обычный провод тут ловил ноль.)
            event->accept();
            return;
        }

        if (m_dragWithShift) {
            // === ГРУППОВОЕ ПЕРЕМЕЩЕНИЕ (Shift) ===
            QPoint adjustedPos = pos;
            if (Component *comp = dynamic_cast<Component*>(m_draggedWire)) {
                QPoint fixedPos = m_draggingStart ? comp->endScenePos().toPoint()
                                                  : comp->startScenePos().toPoint();
                adjustedPos = ensureMinComponentLength(comp->componentType(), fixedPos, pos);
            }
            // ВСЕ провода узла (включая основной) получают ОДНУ позицию — отрыв невозможен
            for (const auto &entry : m_connectedWiresAtPress) {
                Wire *w = entry.first;
                if (!scene()->items().contains(w)) continue;
                if (entry.second) w->setStartPos(adjustedPos);
                else              w->setEndPos(adjustedPos);
            }
        } else {
            // === ОБЫЧНОЕ ПЕРЕТАСКИВАНИЕ (без Shift) ===
            if (Component *comp = dynamic_cast<Component*>(m_draggedWire)) {
                QPoint fixedPos = m_draggingStart ? comp->endScenePos().toPoint()
                                                  : comp->startScenePos().toPoint();
                pos = ensureMinComponentLength(comp->componentType(), fixedPos, pos);
            }
            if (m_draggingStart) m_draggedWire->setStartPos(pos);
            else                 m_draggedWire->setEndPos(pos);
        }

        viewport()->update();
        event->accept();
        return;
    }

    if (m_wireDrawing && m_previewLine) {
        QPoint currentPos = snapToGrid(mapToScene(event->pos()));
        if (m_componentMode) {
            currentPos = ensureMinComponentLength(m_componentType,
                                                  m_wireStartPin.standalonePos,
                                                  currentPos);
        }
        QLineF line = m_previewLine->line();
        line.setP2(currentPos);
        m_previewLine->setLine(line);
        viewport()->update();
        event->accept();
        return;
    }

    if (!m_panning && !m_wireDrawing && !m_draggedWire && m_movingWires.isEmpty()) {
        const QPointF scenePos = mapToScene(event->pos());
        Wire *wireUnderCursor = nullptr;
        // ВАЖНО: IntersectsItemShape — фильтр по ФОРМЕ отрисовки, а не по габаритному прямоугольнику
        const QList<QGraphicsItem*> items = scene()->items(scenePos, Qt::IntersectsItemShape,
                                                            Qt::DescendingOrder, transform());
        for (QGraphicsItem *it : items) {
            if (Wire *w = dynamic_cast<Wire*>(it)) {
                if (w->shape().contains(w->mapFromScene(scenePos))) {
                    wireUnderCursor = w;
                    break;
                }
            }
        }

        if (wireUnderCursor != m_hoveredWire) {
            clearHover();
            m_hoveredWire = wireUnderCursor;
            if (m_hoveredWire) {
                m_hoveredWire->setHovered(true);
                m_hoveredWire->setZValue(10);
            }
        }
    } else {
        clearHover();
    }

    QGraphicsView::mouseMoveEvent(event);
}

void InfiniteGraphicsView::undo() {
    qDebug() << "InfiniteGraphicsView::undo() called";
    if (m_undoStack) m_undoStack->undo();
}
void InfiniteGraphicsView::redo() { if (m_undoStack) m_undoStack->redo(); }
bool InfiniteGraphicsView::canUndo() const { return m_undoStack && m_undoStack->canUndo(); }
bool InfiniteGraphicsView::canRedo() const { return m_undoStack && m_undoStack->canRedo(); }
QString InfiniteGraphicsView::undoText() const { return m_undoStack ? m_undoStack->undoText() : QString(); }
QString InfiniteGraphicsView::redoText() const { return m_undoStack ? m_undoStack->redoText() : QString(); }

Wire* InfiniteGraphicsView::findWireEndpointNear(const QPoint &scenePos, bool *isStart) {
    if (!scene()) return nullptr;
    qreal scale = transform().m11();
    qreal tolerance = kEndpointPixelTolerance / std::max(scale, 0.0001);

    Wire *best = nullptr;
    qreal bestDist = tolerance;

    const QList<QGraphicsItem*> items = scene()->items();
    for (QGraphicsItem *item : items) {
        Wire *w = dynamic_cast<Wire*>(item);
        if (!w) continue;

        qreal ds = QLineF(scenePos, w->startScenePos()).length();
        if (ds < bestDist) { bestDist = ds; best = w; if (isStart) *isStart = true; }

        qreal de = QLineF(scenePos, w->endScenePos()).length();
        if (de < bestDist) { bestDist = de; best = w; if (isStart) *isStart = false; }
    }
    return best;
}

void InfiniteGraphicsView::keyPressEvent(QKeyEvent *event) {
    // ==============================================================
    // 1. ОТМЕНА ДЕЙСТВИЯ (Ctrl+Z)
    // ==============================================================
    if (event->matches(QKeySequence::Undo)) {
        qDebug() << "[KEY] Ctrl+Z pressed. Stack count:" << (m_undoStack ? m_undoStack->count() : 0);
        if (m_undoStack) m_undoStack->undo(); // Откатываем последнюю команду в стеке
        event->accept();
        return;
    }

    // ==============================================================
    // 2. ПОВТОР ДЕЙСТВИЯ (Ctrl+Y на Windows / Ctrl+Shift+Z на Linux)
    // ==============================================================
    if (event->matches(QKeySequence::Redo)) {
        if (m_undoStack) m_undoStack->redo(); // Повторяем отмененную команду
        event->accept();
        return;
    }

    // ==============================================================
    // 3. КЛАВИША ESCAPE (СБРОС И ОТМЕНА)
    // ==============================================================
    if (event->key() == Qt::Key_Escape) {
        // Если сейчас рисуется провод, удаляем временную линию предпросмотра
        if (m_wireDrawing && m_previewLine) {
            scene()->removeItem(m_previewLine);
            delete m_previewLine;
            m_previewLine = nullptr;
            m_wireDrawing = false;
        }
        // Выключаем все активные режимы инструментов (провод, компонент)
        if (m_wireMode) setWireMode(false);
        if (m_componentMode) disableComponentMode();
        // Снимаем всё выделение со сцены
        if (scene()) scene()->clearSelection();
        m_connectedWiresAtPress.clear();
        event->accept();
        return;
    }

    // ==============================================================
    // 4. ВЫДЕЛИТЬ ВСЁ (Ctrl+A)
    // ==============================================================
    if (event->matches(QKeySequence::SelectAll)) {
        if (scene()) {
            // Проходим по всем объектам сцены, которые можно выделить, и выделяем их
            for (QGraphicsItem *item : scene()->items()) {
                if (item->flags() & QGraphicsItem::ItemIsSelectable)
                    item->setSelected(true);
            }
        }
        event->accept();
        return;
    }

    // ==============================================================
    // 5. УДАЛЕНИЕ ВЫДЕЛЕННОГО (Delete или Backspace)
    // ==============================================================
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        // Если сейчас идет перетаскивание (провода или конца провода) — удаление блокируется
        if (m_draggedWire != nullptr || !m_movingWires.isEmpty()) {
            event->accept();
            return;
        }
        // ФИЧА: Если ничего не выделено, но курсор наведён на объект, временно выделяем его
        if (scene() && scene()->selectedItems().isEmpty() && m_hoveredWire) {
            scene()->clearSelection();
            m_hoveredWire->setSelected(true);
        }
        deleteSelectedItems(); // Удаляем выделенные объекты и добавляем команду в Undo
        event->accept();
        return;
    }

    // ==============================================================
    // 6. КОПИРОВАНИЕ (Ctrl+C)
    // ==============================================================
    if (event->matches(QKeySequence::Copy)) {
        copySelected();
        event->accept();
        return;
    }

    // ==============================================================
    // 7. ВЫРЕЗАТЬ (Ctrl+X)
    // ==============================================================
    if (event->matches(QKeySequence::Cut)) {
        cutSelected();
        event->accept();
        return;
    }

    // ==============================================================
    // 8. ВСТАВИТЬ (Ctrl+V)
    // ==============================================================
    if (event->matches(QKeySequence::Paste)) {
        pasteClipboard();
        event->accept();
        return;
    }

    // ==============================================================
    // 00. ПЕРЕМЕЩЕНИЕ СТРЕЛКАМИ ИЛИ ПАНОРАМИРОВАНИЕ (← ↑ → ↓)
    // ==============================================================
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
        event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
    {
        QPoint delta = countSteps(event); // Вычисляем шаг (базовый 50, с Shift - 200)
        QList<QGraphicsItem*> selected = scene() ? scene()->selectedItems() : QList<QGraphicsItem*>();

        // Если ничего не выделено, двигаем объект, на который наведен курсор (m_hoveredWire)
        if (selected.isEmpty() && m_hoveredWire) {
            selected.append(m_hoveredWire);
        }

        if (!selected.isEmpty()) {
            // Если есть объекты, сдвигаем их координаты на delta и создаём команду в Undo
            QVector<MoveItemsCommand::Entry> entries;
            for (QGraphicsItem *item : selected) {
                Wire *wire = dynamic_cast<Wire*>(item);
                if (!wire) continue;
                QPoint oldStart = wire->startScenePos().toPoint();
                QPoint oldEnd   = wire->endScenePos().toPoint();
                QPoint newStart = snapToGrid(oldStart + delta);
                QPoint newEnd   = snapToGrid(oldEnd + delta);
                if (newStart == oldStart && newEnd == oldEnd)
                    continue;
                MoveItemsCommand::Entry entry;
                entry.wire = wire;
                entry.oldStart = QPointF(oldStart);
                entry.oldEnd = QPointF(oldEnd);
                entry.newStart = QPointF(newStart);
                entry.newEnd = QPointF(newEnd);
                entries.append(entry);
            }
            if (!entries.isEmpty()) {
                m_undoStack->push(new MoveItemsCommand(scene(), entries));
                viewport()->update();
            }
        } else {
            // Если объектов нет, панорамируем камеру (двигаем центр сцены)
            QPoint center = mapToScene(viewport()->rect().center()).toPoint();
            center += delta;
            centerOn(center);
        }

        event->accept();
        return;
    }
}

QPoint InfiniteGraphicsView::countSteps(QKeyEvent *event) {
    qreal step = 50.0;
    if (event->modifiers() & Qt::ShiftModifier)
        step *= 4.0;
    QPoint delta;
    switch (event->key()) {
    case Qt::Key_Left:  delta = QPoint(-step, 0); break;
    case Qt::Key_Right: delta = QPoint( step, 0); break;
    case Qt::Key_Up:    delta = QPoint(0, -step); break;
    case Qt::Key_Down:  delta = QPoint(0,  step); break;
    default: break;
    }
    return delta;
}

QPoint InfiniteGraphicsView::findFreePosition(const QPoint &desiredStart, const QPoint &delta, qreal step) {
    if (!scene()) return desiredStart;

    QList<QLineF> existingLines;
    const QList<QGraphicsItem*> items = scene()->items();
    for (QGraphicsItem *item : items) {
        if (Wire *w = dynamic_cast<Wire*>(item)) {
            existingLines.append(QLineF(w->startScenePos(), w->endScenePos()));
        }
    }

    auto isOccupied = [&](const QPoint &newStart) -> bool {
        QPoint newEnd = newStart + delta;
        for (const QLineF &line : existingLines) {
            if (QLineF(newStart, line.p1()).length() < 5.0 ||
                QLineF(newStart, line.p2()).length() < 5.0 ||
                QLineF(newEnd, line.p1()).length() < 5.0 ||
                QLineF(newEnd, line.p2()).length() < 5.0) {
                return true;
            }
            QPointF intersection;
            QLineF::IntersectType type = line.intersects(QLineF(newStart, newEnd), &intersection);
            if (type == QLineF::BoundedIntersection) {
                return true;
            }
        }
        return false;
    };

    for (int i = 0; i < 100; ++i) {
        QPoint candidateRight = desiredStart + QPoint(i * step, 0);
        if (!isOccupied(candidateRight)) {
            return candidateRight;
        }
        if (i != 0) {
            QPoint candidateLeft = desiredStart + QPoint(-i * step, 0);
            if (!isOccupied(candidateLeft)) {
                return candidateLeft;
            }
        }
    }

    qDebug() << "findFreePosition: no free position found, returning desiredStart";
    return desiredStart;
}

void InfiniteGraphicsView::leaveEvent(QEvent *event) {
    clearHover();
    QGraphicsView::leaveEvent(event);
}

void InfiniteGraphicsView::setComponentMode(ComponentType type) {
    if (m_wireMode) setWireMode(false);
    m_componentMode = true;
    m_componentType = type;
    setCursor(Qt::CrossCursor);
}

void InfiniteGraphicsView::disableComponentMode() {
    m_componentMode = false;
    setCursor(m_wireMode ? Qt::CrossCursor : Qt::ArrowCursor);
}

void InfiniteGraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsView::mouseDoubleClickEvent(event);
        return;
    }
    QPointF scenePos = mapToScene(event->pos());
    Component *comp = dynamic_cast<Component*>(scene()->itemAt(scenePos, QTransform()));
    if (!comp) {
        QGraphicsView::mouseDoubleClickEvent(event);
        return;
    }

    // ================= РЕЗИСТОР =================
    if (comp->componentType() == ComponentType::Resistor) {
        QVector<ParamField> fields;
        fields.append({ "Сопротивление (Ом, k, M):", comp->resistance(),
                       0.0, 1e9, 0, 1.0, ParamField::ResText });
        fields.append({ "Мощность рассеивания (Вт):", comp->param("power").toDouble(),
                       0.0, 1000.0, 3, 0.125, ParamField::Spin });
        fields.append({ "Процент точности (%):", comp->param("accuracy").toDouble(),
                       0.0, 100.0, 1, 1.0, ParamField::Spin });

        showComponentParamsDialog(this, "Параметры резистора", fields,
                                  [this, comp](const QVector<double> &v) {
                                      const double oldRes = comp->resistance();
                                      const double oldPow = comp->param("power").toDouble();
                                      const double oldAcc = comp->param("accuracy").toDouble();
                                      if (qAbs(v[0]-oldRes) < 1e-6 && qAbs(v[1]-oldPow) < 1e-6 && qAbs(v[2]-oldAcc) < 1e-6)
                                          return;
                                      m_undoStack->beginMacro("Изменение параметров резистора");
                                      m_undoStack->push(new ChangeParamCommand(comp, "resistance", oldRes, v[0]));
                                      comp->setResistance(v[0]);
                                      m_undoStack->push(new ChangeParamCommand(comp, "power", oldPow, v[1]));
                                      comp->setParam("power", v[1]);
                                      m_undoStack->push(new ChangeParamCommand(comp, "accuracy", oldAcc, v[2]));
                                      comp->setParam("accuracy", v[2]);
                                      m_undoStack->endMacro();
                                  });
        event->accept();
        return;
    }

    // ================= КОНДЕНСАТОР =================
    if (comp->componentType() == ComponentType::Non_Polar_Capacitor) {
        const double curV = comp->param("voltage").toDouble();
        QVector<ParamField> fields;
        fields.append({ "Ёмкость:", comp->capacitance(),
                       0.0, 1.0, 0, 0.0, ParamField::CapText });
        fields.append({ "Напряжение (В):", curV > 0 ? curV : 50.0,
                       1.0, 10000.0, 1, 1.0, ParamField::Spin });
        fields.append({ "Точность (%):", comp->param("tolerance").toDouble(),
                       0.0, 100.0, 1, 1.0, ParamField::Spin });
        fields.append({ "ESR (Ом):", comp->param("esr").toDouble(),
                       0.0, 1000.0, 3, 0.1, ParamField::Spin });

        showComponentParamsDialog(this, "Параметры конденсатора", fields,
                                  [this, comp](const QVector<double> &v) {
                                      const double oldC = comp->capacitance();
                                      const double oldV = comp->param("voltage").toDouble();
                                      const double oldT = comp->param("tolerance").toDouble();
                                      const double oldE = comp->param("esr").toDouble();
                                      if (qAbs(v[0]-oldC) < 1e-15 && qAbs(v[1]-oldV) < 1e-6 &&
                                          qAbs(v[2]-oldT) < 1e-6  && qAbs(v[3]-oldE) < 1e-6)
                                          return;
                                      m_undoStack->beginMacro("Изменение параметров конденсатора");
                                      m_undoStack->push(new ChangeParamCommand(comp, "capacitance", oldC, v[0]));
                                      comp->setCapacitance(v[0]);
                                      m_undoStack->push(new ChangeParamCommand(comp, "voltage", oldV, v[1]));
                                      comp->setParam("voltage", v[1]);
                                      m_undoStack->push(new ChangeParamCommand(comp, "tolerance", oldT, v[2]));
                                      comp->setParam("tolerance", v[2]);
                                      m_undoStack->push(new ChangeParamCommand(comp, "esr", oldE, v[3]));
                                      comp->setParam("esr", v[3]);
                                      m_undoStack->endMacro();
                                  });
        event->accept();
        return;
    }

    // ================= ПОЛЯРНЫЙ КОНДЕНСАТОР =================
    if (comp->componentType() == ComponentType::Polar_Capacitor) {
        const double curV = comp->param("voltage").toDouble();
        QVector<ParamField> fields;
        fields.append({ "Ёмкость:", comp->capacitance(), 0.0, 1.0, 0, 0.0, ParamField::CapText });
        fields.append({ "Напряжение (В):", curV > 0 ? curV : 25.0, 1.0, 10000.0, 1, 1.0, ParamField::Spin });
        fields.append({ "Точность (%):", comp->param("tolerance").toDouble(), 0.0, 100.0, 1, 1.0, ParamField::Spin });
        fields.append({ "ESR (Ом):", comp->param("esr").toDouble(), 0.0, 1000.0, 3, 0.1, ParamField::Spin });
        fields.append({ "Ток утечки (мкА):", comp->param("leakage_uA").toDouble(), 0.0, 10000.0, 1, 1.0, ParamField::Spin }); // <-- Новое поле

        showComponentParamsDialog(this, "Параметры полярного конденсатора", fields,
                                  [this, comp](const QVector<double> &v) {
                                      const double oldC = comp->capacitance();
                                      const double oldV = comp->param("voltage").toDouble();
                                      const double oldT = comp->param("tolerance").toDouble();
                                      const double oldE = comp->param("esr").toDouble();
                                      const double oldL = comp->param("leakage_uA").toDouble();

                                      if (qAbs(v[0]-oldC) < 1e-15 && qAbs(v[1]-oldV) < 1e-6 &&
                                          qAbs(v[2]-oldT) < 1e-6  && qAbs(v[3]-oldE) < 1e-6 && qAbs(v[4]-oldL) < 1e-6)
                                          return;

                                      m_undoStack->beginMacro("Изменение параметров полярного конденсатора");
                                      m_undoStack->push(new ChangeParamCommand(comp, "capacitance", oldC, v[0]));
                                      comp->setCapacitance(v[0]);
                                      m_undoStack->push(new ChangeParamCommand(comp, "voltage", oldV, v[1]));
                                      comp->setParam("voltage", v[1]);
                                      m_undoStack->push(new ChangeParamCommand(comp, "tolerance", oldT, v[2]));
                                      comp->setParam("tolerance", v[2]);
                                      m_undoStack->push(new ChangeParamCommand(comp, "esr", oldE, v[3]));
                                      comp->setParam("esr", v[3]);
                                      m_undoStack->push(new ChangeParamCommand(comp, "leakage_uA", oldL, v[4])); // <-- Сохраняем
                                      comp->setParam("leakage_uA", v[4]);
                                      m_undoStack->endMacro();
                                  });
        event->accept();
        return;
    }

    QGraphicsView::mouseDoubleClickEvent(event);
}
void InfiniteGraphicsView::copySelected() {
    if (!scene()) return;
    if (scene()->selectedItems().isEmpty() && m_hoveredWire) {
        scene()->clearSelection();
        m_hoveredWire->setSelected(true);
    }
    m_clipboard.clear();
    m_pasteOffset = QPoint(0, 0);
    m_hasPasted = false;
    const QList<QGraphicsItem*> items = scene()->selectedItems();
    QList<Wire*> wires;
    for (QGraphicsItem *item : items) {
        if (Wire *w = dynamic_cast<Wire*>(item)) wires.append(w);
    }
    if (wires.isEmpty()) return;
    QPoint center(0,0);
    int count = 0;
    for (Wire *w : wires) {
        center += w->startScenePos().toPoint();
        center += w->endScenePos().toPoint();
        count += 2;
    }
    if (count > 0) center /= count;
    m_clipboardAnchor = center;
    for (Wire *w : wires) {
        CopiedWireData data;
        data.relativeStart = w->startScenePos().toPoint() - center;
        data.relativeEnd   = w->endScenePos().toPoint() - center;
        data.startConnected = w->isStartConnected();
        data.endConnected = w->isEndConnected();
        Component *comp = dynamic_cast<Component*>(w);
        if (comp) {
            data.isComponent = true;
            data.componentType = comp->componentType();
            data.params = comp->params();
        }
        m_clipboard.append(data);
    }
    auto *mime = new QMimeData;
    mime->setData(kClipboardMime, serializeClipboard(m_clipboard));
    QApplication::clipboard()->setMimeData(mime);
}

void InfiniteGraphicsView::cutSelected() {
    if (!scene()) return;
    if (scene()->selectedItems().isEmpty() && m_hoveredWire) {
        scene()->clearSelection();
        m_hoveredWire->setSelected(true);
    }
    copySelected();
    deleteSelectedItems();
}

void InfiniteGraphicsView::pasteClipboard() {
    if (const QMimeData *mime = QApplication::clipboard()->mimeData();
        mime && mime->hasFormat(kClipboardMime)) {
        m_clipboard = deserializeClipboard(mime->data(kClipboardMime));
    }
    if (!scene() || m_clipboard.isEmpty()) return;
    int minX = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::lowest();
    int minY = std::numeric_limits<int>::max();
    int maxY = std::numeric_limits<int>::lowest();
    for (const CopiedWireData &data : m_clipboard) {
        minX = qMin(minX, qMin(data.relativeStart.x(), data.relativeEnd.x()));
        maxX = qMax(maxX, qMax(data.relativeStart.x(), data.relativeEnd.x()));
        minY = qMin(minY, qMin(data.relativeStart.y(), data.relativeEnd.y()));
        maxY = qMax(maxY, qMax(data.relativeStart.y(), data.relativeEnd.y()));
    }
    int groupWidth  = maxX - minX;
    int groupHeight = maxY - minY;

    m_undoStack->beginMacro("Вставка группы проводов");
    QPoint cursorPos = snapToGrid(mapToScene(mapFromGlobal(QCursor::pos())));
    if (m_hasPasted) {
        int deltaY = qAbs(cursorPos.y() - m_lastPastePos.y());
        int threshold = qMax(static_cast<int>(groupHeight * 0.8), 30);
        if (deltaY > threshold) m_pasteOffset = QPoint(0, 0);
    }
    QPoint newAnchor = cursorPos + m_pasteOffset;
    QList<QGraphicsItem*> pastedItems;
    for (const CopiedWireData &data : m_clipboard) {
        QPoint newStart = snapToGrid(newAnchor + data.relativeStart);
        QPoint newEnd   = snapToGrid(newAnchor + data.relativeEnd);
        Pin newStartPin; newStartPin.owner = nullptr; newStartPin.standalonePos = newStart;
        Pin newEndPin;   newEndPin.owner   = nullptr; newEndPin.standalonePos   = newEnd;
        QGraphicsItem *newItem = nullptr;
        if (data.isComponent) {
            Component *comp = new Component(newStartPin, newEndPin, data.componentType);
            comp->setParams(data.params);
            comp->setStartConnected(data.startConnected);
            comp->setEndConnected(data.endConnected);
            m_undoStack->push(new AddComponentCommand(scene(), comp));
            newItem = comp;
        } else {
            Wire *wire = new Wire(newStartPin, newEndPin);
            wire->setStartConnected(data.startConnected);
            wire->setEndConnected(data.endConnected);
            m_undoStack->push(new AddWireCommand(scene(), wire));
            newItem = wire;
        }
        pastedItems.append(newItem);
    }
    m_undoStack->endMacro();
    recalculateAllWireConnections(scene());
    m_pasteOffset += QPoint(groupWidth + 20, 0);
    m_lastPastePos = cursorPos;
    m_hasPasted = true;
    scene()->clearSelection();
    for (QGraphicsItem *item : pastedItems) item->setSelected(true);
}

void InfiniteGraphicsView::setPreferredCursor(Qt::CursorShape shape)
{
    m_preferredCursor = shape;
    setCursor(shape);
}