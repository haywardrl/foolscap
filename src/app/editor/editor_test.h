#pragma once
#include "app/buffer/buffer.h"
#include "editor.h"

const buffer_t *editor_test_get_buffer(const editor_t *ed);

// undo/redo stack depths, for asserting recording behaviour
size_t editor_test_undo_count(const editor_t *ed);
size_t editor_test_redo_count(const editor_t *ed);
