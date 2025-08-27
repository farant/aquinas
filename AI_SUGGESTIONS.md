# AI Suggestions for Aquinas OS

This file contains suggestions for improvements and enhancements noticed while working on the codebase. These are ideas beyond what has been explicitly requested.

## Suggestions

---

### 1. Extract Stack Monitoring to Debug Module
**Description:** The stack monitoring functions (get_esp, get_stack_usage, get_max_stack_usage) could be moved to a debug.c/h module along with the stack reporting logic from the main loop.

**Why beneficial:**
- Cleaner separation of debug/monitoring code
- Makes kernel.c more focused on core functionality
- Could add more debug utilities in one place

**Implementation approach:**
- Create debug.c/h with stack monitoring functions
- Move periodic stack reporting from main loop
- Could add memory usage reporting, performance counters

---

### 2. Create Initialization Module
**Description:** The initialization sequence in kernel_main could be extracted to an init.c module that handles all subsystem initialization in order.

**Why beneficial:**
- Simplifies kernel_main
- Makes dependencies between subsystems clearer
- Easier to modify init order if needed

**Implementation approach:**
- Create init.c with init_all() function
- Move all init_* calls there
- Document initialization order dependencies

---

### 3. Mouse Acceleration Curve
**Description:** Implement a simple acceleration curve for mouse movement instead of linear scaling.

**Why beneficial:**
- More precise control for small movements
- Faster navigation for large movements
- Better user experience overall

**Implementation approach:**
- Apply a power curve to accumulated mouse deltas
- Allow fine-tuning through constants

---

### 4. Page Metadata Structure
**Description:** Add metadata to pages (creation time, last modified, word count, etc.)

**Why beneficial:**
- Useful for the future object database system
- Could display stats in nav bar
- Foundation for versioning system

**Implementation approach:**
- Extend Page struct with metadata fields
- Update display to show relevant info

---

### 5. Keyboard Repeat Rate
**Description:** Implement key repeat when holding down a key.

**Why beneficial:**
- Standard behavior users expect
- Makes deletion and navigation faster
- Easy to implement with timer

**Implementation approach:**
- Track key down time
- Trigger repeat after initial delay
- Use shorter delay for subsequent repeats

---

### 6. Visual Bell
**Description:** Flash the screen border or change colors briefly for alerts instead of no feedback.

**Why beneficial:**
- User feedback when operations fail (like page full)
- Non-intrusive notification system
- Could use different colors for different events

**Implementation approach:**
- Temporarily change border color
- Restore after a few polling cycles


---

### 7. Centralized Mouse Handling System
**Description:** Refactor mouse handling into a centralized driver instead of duplicating protocol parsing in each demo.

**Why beneficial:**
- Eliminates code duplication between layout_demo.c and dispi_demo.c
- Provides consistent event generation (MOUSE_DOWN, MOUSE_UP, MOUSE_MOVE)
- Clearer coordinate system management
- Single source of truth for mouse state
- Simpler integration for new UI components

**Implementation approach:**
- Create mouse.c/h with centralized MouseState structure
- Implement mouse_poll() to handle serial protocol parsing
- Generate proper mouse events with state change detection
- Fix coordinate confusion between pixels and regions in view/layout
- Unify cursor rendering through mouse_cursor module
- Register event callbacks instead of inline protocol handling

**Key improvements:**
- Proper button state tracking (current vs previous)
- Automatic MOUSE_UP event generation
- MOUSE_MOVE events for hover effects
- Clear separation of driver logic from application code

---

## Notes

These suggestions are based on patterns observed in the code and potential improvements that would enhance the system while maintaining its simplicity and design philosophy.
