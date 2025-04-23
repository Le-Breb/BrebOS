#include "progress_bar_user.h"
#include "../core/fb.h"

#include <kstddef.h>

Progress_bar_user::Progress_bar_user()
{
    FB::try_acquire_progress_bar(this);
}

Progress_bar_user::~Progress_bar_user()
{
    FB::release_progres_bar(this);
}

void Progress_bar_user::update_progress(uint new_value, uint total) const
{
    char progress = (char)(((float)new_value * 100.f) / (float)total);
    FB::update_progress_bar(progress, this);
}
