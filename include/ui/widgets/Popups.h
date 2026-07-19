#pragma once

#include <cstddef>

namespace tsm { namespace ui { namespace widgets {


bool ConfirmPopup(const char* popup_id,
                  const char* title,
                  const char* message,
                  const char* confirm_label = "Confirm",
                  const char* cancel_label = "Cancel");

bool InputPopup(const char* popup_id,
                const char* title,
                const char* placeholder,
                char* buf,
                size_t buf_size,
                const char* confirm_label = "OK",
                const char* cancel_label = "Cancel");

bool ContextMenu(const char* popup_id,
                 const char* title,
                 const char* const* options,
                 int options_count,
                 int* selected_index,
                 bool* include_flag = nullptr,
                 const char* include_label = nullptr);

bool DetectOptionHold(const char* id, bool hovered, float seconds = 1.0f);

bool InputWithPlaceholder(const char* id, char* buf, size_t buf_size, const char* placeholder);

}}}
