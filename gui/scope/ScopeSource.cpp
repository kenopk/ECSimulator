#include "scope/ScopeSource.h"

#include <QFile>
#include <QTextStream>
#include <cmath>

#include "items/CircuitItem.h"

ScopeSource::ScopeSource(CircuitItem *target, QObject *parent)
    : QObject(parent), m_target(target)
{
    if (target)
        m_title = target->displayName();
}

void ScopeSource::setSettings(const ScopeSettings &s)
{
    m_settings = s;
    emit changed();
}

void ScopeSource::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    emit changed();
}

void ScopeSource::push(double t, double v, double i)
{
    if (!m_enabled)
        return;

    // Если время не росло (расчёт начали заново), начинаем запись с чистого листа.
    if (!m_samples.isEmpty() && t < m_samples.last().t) {
        m_samples.clear();
        m_maxV = m_maxI = 0;
    }

    m_samples.append({ t, v, i });
    m_maxV = std::max(m_maxV, std::fabs(v));
    m_maxI = std::max(m_maxI, std::fabs(i));
    if (m_samples.size() > kMaxSamples + kMaxSamples / 4)
        m_samples.remove(0, m_samples.size() - kMaxSamples);
    emit changed();
}

void ScopeSource::clear()
{
    m_samples.clear();
    m_maxV = m_maxI = 0;
    emit changed();
}

bool ScopeSource::exportCsv(const QString &path, QString *error) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    QTextStream out(&f);
    out.setRealNumberPrecision(9);
    out << "t,V,I\n";
    for (const ScopeSample &s : m_samples)
        out << s.t << ',' << s.v << ',' << s.i << '\n';
    return true;
}

void ScopeSource::targetGone()
{
    m_target = nullptr;
    emit moved();
    emit changed();
}
