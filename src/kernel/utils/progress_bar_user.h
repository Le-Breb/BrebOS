#ifndef PROGRESS_BAR_USER_H
#define PROGRESS_BAR_USER_H
#include "kstddef.h"

class Progress_bar_user {
protected:
    Progress_bar_user();

    ~Progress_bar_user();

    void update_progress(uint new_value, uint total) const;
};



#endif //PROGRESS_BAR_USER_H
