#pragma once

// Reset the mock framebuffer to a known initial state (128 = gray).
// Call from setUp() in tests that need fresh pixel state.
void display_mock_reset(void);
