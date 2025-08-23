
# Known Bugs in Aquinas OS

This file tracks known bugs that need to be addressed. Once fixed, move them to BUGS_FIXED.md.

## Current Bugs

### 1. Highlight Loses Position After Typing
**Description:** If you have a word that is highlighted and then you start inserting text before the word, the highlight gets pushed off the position of the word.

**How to reproduce:**
1. Click on a word to highlight it
2. Move cursor to a position before the highlighted word
3. Start typing new text
4. The highlight indices don't update, causing highlight to drift

**Expected behavior:** Highlight should stay on the same word even when text is inserted before it.

**Actual behavior:** Highlight stays at the same character indices, which now point to different text.

**Code location:** `kernel.c` - `insert_char()` and `delete_char()` functions

**Potential fix:** When inserting/deleting characters, adjust `highlight_start` and `highlight_end` indices:
- If inserting before highlight: increment both indices
- If deleting before highlight: decrement both indices
- If inserting/deleting within highlight: clear the highlight

---

### 2. Tab Character Display Width Issue
**Description:** When clicking on text after a tab character, the click position calculation may be off by one or more characters.

**How to reproduce:**
1. Type some text with tabs in it
2. Click on words after the tab
3. The wrong word may be highlighted

**Expected behavior:** Clicking on a word should highlight that exact word.

**Actual behavior:** Words after tabs may have incorrect click targets.

**Code location:** `kernel.c` - `poll_mouse()` function, buffer position calculation section

**Potential fix:** The tab handling in click position calculation needs to account for tabs being 2 visual spaces but only 1 buffer position.

---

### 3. Page Boundary Line Wrap
**Description:** When text wraps at exactly 80 characters, the cursor position calculation may be incorrect.

**How to reproduce:**
1. Type text that reaches exactly column 80
2. Continue typing to wrap to next line
3. Try to navigate with arrow keys

**Expected behavior:** Cursor should move smoothly between wrapped lines.

**Actual behavior:** Cursor may jump unexpectedly or be positioned incorrectly.

**Code location:** `kernel.c` - cursor movement functions

**Potential fix:** Review line wrap handling in cursor up/down movement.

---

## Notes

Please add new bugs as you discover them, following the format above. Include as much detail as possible to help with reproduction and fixing.
