# Aquinas OS Goals

This document outlines the long-term vision and goals for Aquinas OS.

## Code Quality & Testing

### 1. Well-Commented Code
- Follow Antirez's comment philosophy (see literature/antirez-comment-philosophy.md)
- Include design comments explaining architecture decisions
- Add teacher comments for complex algorithms and protocols
- Document the "why" behind non-obvious implementation choices
- Use guide comments to improve code readability

### 2. Comprehensive Testing
- Implement unit tests for individual modules
- Add integration tests to catch regressions
- Test edge cases and error conditions
- Ensure tests run in the build process to catch bugs early

### 3. Modular Architecture
- Break code into testable, independent modules
- Separate concerns (e.g., rendering, input handling, data management)
- Use clear interfaces between modules
- Allow modules to be tested in isolation

## Performance & Rendering

### 4. Efficient Rendering
- Implement differential rendering - only update changed characters
- Track dirty regions to minimize VGA buffer writes
- Optimize for common editing patterns (typing, backspace, cursor movement)
- Avoid full-screen refreshes when possible

## User Interface & Interaction

### 5. Pagination-Only Display
- No scrolling - only pagination for all content
- Fixed-width fonts only (no variable width fonts)
- Scrolling and variable width fonts were a mistake
- Variable width fonts make pagination much harder
- Pagination provides better object permanence - content stays where you put it
- Every page has a fixed position, making navigation predictable

### 6. Vim Key Bindings
- Implement modal editing (normal and insert modes)
- Support common vim navigation (hjkl, w, b, e, gg, G, etc.)
- Maintain compatibility with existing mouse support

### 7. Object Database Filesystem
- Going to try not having files!
- Move beyond traditional file/directory hierarchy
- Implement object-based storage with relationships
- Support versioning and event sourcing
- Allow queries and graph-like navigation between objects
- Inspired by object databases and graph databases

### 8. Executable Text Commands
- Implement clickable/executable commands in the editor (like Acme/Oberon)
- Shift click to execute text as commands
- Commands can operate on selected text or current context
- Extensible command system for user-defined operations
- Text becomes the interface for system control

### 9. Split Pane Support
- Allow horizontal splitting of pages into two panes
- Never going to use scrolling! Only pagination
- Commands to split, unsplit, and navigate between panes
- Useful for different kind of views
- Possibly panes will have their own pagination structure

## Networking & Communication

### 10. Simple Network Protocol
- Design a minimal protocol for host communication
- Not necessarily TCP/IP - could be simpler serial-based protocol
- Support sending and receiving text/data from host system
- Enable sending strings to host and receiving strings from host
- Keep protocol simple and documented

## Implementation Priority

The goals are roughly ordered by priority, with code quality and testing being foundational. Performance improvements and advanced features can be built on top of a solid, well-tested base.

## Design Principles

- **Simplicity**: Keep implementations simple and understandable
- **Reliability**: Prioritize stability over features
- **Documentation**: Every feature should be well-documented
- **User Control**: Give users direct manipulation of their data
- **Integration**: Features should work together harmoniously
