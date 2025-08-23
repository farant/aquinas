# AI Suggestions for Aquinas OS

This file contains suggestions for improvements and enhancements noticed while working on the codebase. These are ideas beyond what has been explicitly requested.

## Suggestions

### 1. Extract VGA Module
**Description:** The VGA-related code (cursor updates, buffer manipulation) could be extracted into a separate `vga.c` module similar to how we extracted `serial.c`.

**Why beneficial:** 
- Better separation of concerns
- Makes the code more testable
- Reduces kernel.c size
- Easier to swap rendering backends in future

**Implementation approach:**
- Create `vga.c` and move VGA buffer operations
- Define clear interface in `vga.h`
- Keep high-level page rendering in kernel.c

---

### 2. Add Bounds Checking Macro
**Description:** Create a bounds checking macro for buffer operations to prevent overflows.

**Why beneficial:**
- Currently we have manual bounds checks scattered through the code
- A macro would ensure consistent checking
- Would make the code more robust against buffer overflows

**Implementation approach:**
```c
#define SAFE_BUFFER_ACCESS(buf, pos, len) ((pos) >= 0 && (pos) < (len))
```

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

---

### 7. Complete C89 Compliance Fixes
**Description:** Finish converting all remaining code to be fully C89 compliant.

**Why beneficial:**
- Currently have partial C89 conversion
- Full compliance ensures maximum portability
- Forces cleaner code structure

**Remaining issues to fix:**
- Mixed declarations in `move_cursor_down()` - need to declare all variables at function start
- Mixed declarations in `poll_mouse()` - several variables declared mid-function
- Check all other functions for mid-block declarations
- Consider whether to keep `-pedantic` flag (very strict) or just use `-std=c89`

**Implementation approach:**
- Systematically go through each function
- Move all variable declarations to the beginning of each block
- Test compilation with `-std=c89 -pedantic -Wall`

---

## Notes

These suggestions are based on patterns observed in the code and potential improvements that would enhance the system while maintaining its simplicity and design philosophy.