#pragma once

class QString;

// Справка по вкладке раздела «Печатные платы» (HTML): первый абзац — краткая подсказка при наведении на «?»,
// остальное открывается по щелчку. pageId — номер из PcbPageId.
QString pcbHelpHtml(int pageId);
