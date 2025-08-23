# Writing Small Programs in C: Lessons from Sean Barrett

## Introduction and Core Philosophy

Sean Barrett, creator of the STB libraries and veteran programmer with 35+ years of experience, presents a compelling case for using C as a primary language for small utility programs—a practice many consider unconventional. His journey from BASIC (1980) through Pascal, C, and C++ (which he used professionally from 1992-2000) before returning to C offers unique insights into effective programming practices.

The central thesis revolves around two fundamental principles:
1. **Spend your time writing code that gets things done** - Eliminate friction and overhead
2. **Code reuse is essential** - Particularly critical in C due to its limited standard library

## The Evolution of Small Program Development

Barrett traces the historical arc of quick programming solutions: from Unix pipes and shell scripts, through Perl (offering better portability than shell scripts), to modern dynamic languages like Python and Ruby. While acknowledging these languages' strengths, he demonstrates that C can achieve comparable conciseness with the right approach.

A pivotal example comes from 2008, when Brad Fitzpatrick posted a Python program to find anagrams in name lists. Barrett's C solution, while slightly longer, achieved similar conciseness through extensive use of his STB library functions—nearly every line prefixed with `stb_`. This illustrated that the perceived verbosity of C stems not from the language itself but from its standard library limitations.

## The STB Libraries Genesis

The STB libraries emerged from a critical realization during Barrett's time at Looking Glass Studios. After writing a coverage tester for the Thief renderer, he later found himself needing to rewrite the same tool at another company—work he'd already done but couldn't reuse due to ownership issues.

This experience crystallized a key principle: **Never write the same utility twice**. Barrett resolved to create reusable, public domain libraries for common programming tasks. As he notes, "a coverage tester is not a core competency of a game company"—there's no competitive advantage in keeping such tools proprietary.

The libraries now comprise:
- 18 libraries
- 45,000 lines of code
- Focus on being easy to deploy (single header files), easy to use (simple APIs), and easy to license (public domain)

## Practical Techniques for Rapid Development

### Streamlining the Development Environment

Barrett demonstrates his "quick.c" workflow—a masterclass in removing friction:

1. **Single file setup**: A persistent `quick.c` file that serves as the starting point for all small programs
2. **Instant deployment**: A batch file (`install q [name]`) that copies the compiled executable to his bin directory
3. **No project overhead**: Bypasses IDE project creation, settings configuration, and other setup tasks

This approach exemplifies the principle of identifying and eliminating non-programming tasks. As Barrett emphasizes, the overhead of creating a new project might be acceptable for a two-year game development, but it's prohibitive for a five-minute utility.

### The Vector Problem and Solution

A breakthrough came with solving C's lack of dynamic arrays (vectors). Barrett developed a technique using macros that allows:

```c
data *foo = NULL;  // Looks like a regular C array
foo[3] = value;    // Works like a regular array
```

This works by storing length and capacity information in memory immediately before the array pointer, accessed through macros like `stb_arr_len()` and `stb_arr_push()`. This single innovation made C viable for rapid development—without it, Barrett states he "would not be comfortable using C."

### Most-Used STB Functions

Analysis of Barrett's actual usage patterns reveals the most valuable utilities:
- **stb_arr_len** and **stb_arr_push**: Dynamic array operations
- **stb_lerp** and **stb_linear_remap**: Mathematical interpolation
- **stb_sdict**: Hash table mapping strings to pointers
- **stb_file_string**: File reading utilities
- **stb_min/max/clamp**: Basic math operations missing from standard C
- **stb_rand/frand**: Better random number generation

## Code Reuse Philosophy

Barrett's approach to code reuse differs from traditional package managers (CPAN, Ruby Gems, etc.). The STB libraries philosophy emphasizes:

1. **Self-contained functionality**: Once you call `stb_image_load`, you're done—the implementation details don't matter
2. **Minimal mental overhead**: Single-file deployment means no complex build system integration
3. **No dependencies**: Each library stands alone

This addresses Fred Brooks' observation about code reuse—the problem isn't just finding existing code, but finding code worth using. STB libraries solve this through focused, high-quality implementations of common needs.

## Live Coding Example: Token Analysis

During the talk, Barrett demonstrates his approach by writing a program to analyze which STB functions he uses most frequently. The development process illustrates several key points:

1. **Pragmatic error handling**: Not every allocation needs freeing in a short-lived program
2. **Iterative debugging**: Two simple bugs (forgetting to increment loop variables) were quickly identified and fixed
3. **Tool selection**: Despite the task being suitable for regex or shell scripts, using C with STB utilities proved efficient

The entire program took about 10 minutes to write, including debugging—demonstrating that C can be viable for quick scripting tasks.

## Controversial Positions and Clarifications

### On the C Standard Library
Unlike some low-level programming advocates, Barrett embraces using the standard library where appropriate: `printf`, `malloc/free`, `realloc`. However, he acknowledges problematic APIs (`strcpy`, `gets`) and supplements with better alternatives.

### On Strict Aliasing and Undefined Behavior
Barrett strongly criticizes compiler optimizations that break the "de facto" C standard—the one programmers actually relied on for decades. His position: working code from 20 years ago isn't "broken" just because a specification says so. The ecosystem consists of both programmers and compiler writers, and the latter shouldn't unilaterally change the rules.

### On Performance vs. Productivity
Barrett accepts small inefficiencies (10% performance hit) for streamlined programming experience, unless it impacts the development iteration cycle. This pragmatic approach contrasts with purist positions demanding maximum optimization always.

## Key Takeaways for Practitioners

1. **Experience the pain personally**: Barrett emphasizes that simply hearing about code reuse benefits isn't enough—you need to experience the pain of rewriting code to truly internalize the lesson. As he notes, it took him 25 years to fully grasp this.

2. **Build your own standard library**: Whether using STB or creating your own, having a personal toolkit of frequently-used utilities is essential for C productivity.

3. **Optimize for the common case**: Make simple things easy. Many APIs fail by making complex things possible but neglecting the simple, common use cases.

4. **Remove friction relentlessly**: Every step between "I need a program" and "I'm writing code" should be examined and potentially eliminated.

5. **Public domain as strategic advantage**: Making utilities public domain benefits everyone, including yourself—you can use them anywhere without legal concerns.

## Future Outlook

While Barrett doesn't see C overtaking higher-level languages for general use, he identifies a growing community of programmers (influenced by projects like Handmade Hero) who understand low-level programming benefits. The goal isn't universal C adoption but rather demonstrating that with proper tooling and methodology, C remains viable for tasks many consider it unsuited for.

The approach scales from five-minute utilities to game jam projects, though benefits diminish with program size. For Barrett, the combination of STB libraries and streamlined workflow makes C not just viable but preferable for small programs—a 36-year journey to optimize the programming experience.
