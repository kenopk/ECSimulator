#pragma once
// Единственное место, где gui подключает типы ядра. Остальные файлы gui
// включают этот заголовок и не знают, где именно лежит ядро.
#include <govno/core/ElementTypes.h>

using govno::core::Category;
using govno::core::ElementType;
using govno::core::ElementInfo;
using govno::core::ParamDef;
using govno::core::allElements;
using govno::core::categoryName;
using govno::core::elementInfo;
