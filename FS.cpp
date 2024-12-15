#include "FS.h"

FS::~FS() = default;

list* FS::fs_list = nullptr;

void FS::init()
{
	fs_list = new list();
}
