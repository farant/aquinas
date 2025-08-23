# The Big OOPs: Anatomy of a Thirty-five-year Mistake - A Historical Analysis

## Introduction: The Architecture Question

In 1998, Looking Glass Studios shipped *Thief: The Dark Project*, and with it, unknowingly delivered what appears to be the first commercial implementation of an entity-component system (ECS). This architectural pattern represented a fundamental departure from the object-oriented programming (OOP) hierarchies that dominated software design thinking at the time. The story of how we got here—and why it took 35 years to rediscover what was already known in 1963—reveals crucial insights about software architecture, the propagation of ideas, and the importance of solving hard problems.

## The Core Mistake: Compile-Time Hierarchies

The central issue isn't with object-oriented programming as a whole, but with a specific architectural pattern: **compile-time hierarchies with encapsulation boundaries that match the domain model**. This approach suggests that if you're modeling cars, trucks, and buses in your software, you should create a hierarchy where `Vehicle` is the parent class, with `Car`, `Truck`, and `Bus` as subclasses, each encapsulating their own data and behavior.

This pattern appears everywhere in programming education today. Search for "OOP tutorial" and you'll find examples of vehicles inheriting from base classes, special Iron Man classes extending regular Iron Man classes, and bicycles branching into mountain bikes and road bikes. But where did this idea come from, and why has it persisted despite its limitations?

## The Historical Thread: From Distributed Systems to Game Engines

### The C++ Origin Story (1978-1985)

Bjarne Stroustrup's journey to creating C++ began not with language design, but with distributed systems research. In 1978, as a PhD student, he needed to simulate distributed computing systems—something practically impossible to test on real hardware at the time. He chose Simula, a simulation language he'd learned from one of its creators, Kristen Nygard.

Stroustrup found Simula's class concept excellent for "direct mapping"—creating classes that directly represented the things he was simulating. Crucially, his domain naturally consisted of encapsulated entities: processes and computers that genuinely couldn't access each other's internals. The abstraction fit perfectly.

However, the reality was harsh. Simula programs suffered from:
- Catastrophic build times (compiling 1/30th of the program and linking was slower than recompiling everything)
- Terrible runtime performance (80% spent in garbage collection even with no garbage to collect)

He had to rewrite everything in BCPL, losing all the type safety he'd valued. This experience directly motivated C with Classes (later C++): capturing Simula's benefits without its performance penalties.

### The Simula Story (1962-1967)

Simula's creators, Ole-Johan Dahl and Kristen Nygard at the Norwegian Computing Center, weren't trying to revolutionize programming paradigms. They were solving a practical problem in operations research: how to simulate systems like toll bridges with different vehicle types (cars, trucks, buses) without duplicating code.

Their solution—subclassing—wasn't even their original idea. They credited C.A.R. Hoare's 1966 paper on record handling, which proposed both:
1. Hierarchical record types (what became class inheritance)
2. Discriminated unions with type-safe switch statements (what Stroustrup later removed from C++)

The irony is profound: Hoare's paper contained both approaches, but only the hierarchy survived the transition through Simula to C++.

### The Real Origin: Plexes and Sketchpad (1960-1963)

The deepest roots trace back to two MIT projects that would fundamentally shape computing:

**Douglas T. Ross and the Plex (1952-1960)**
At MIT's Servo Mechanisms Laboratory, Ross developed "plexes"—structured data records that predated COBOL's records and included:
- Data members (coordinates, values)
- Pointers to other plexes
- Flags for conditional processing
- Function pointers (addresses of subroutines to jump to)

This was essentially the "fat struct" approach with function pointers—in 1960, contemporaneous with LISP.

**Ivan Sutherland's Sketchpad (1960-1963)**
Sutherland's Sketchpad, which Alan Kay called "the most significant single thesis ever done," implemented a drawing program with revolutionary features:
- Constraint solving (making lines perpendicular, circles tangent)
- Direct manipulation with a light pen
- The concept of moving objects rather than erasing and redrawing

The crucial architectural insight: Sketchpad used a "ring structure" where:
- Entities were just IDs
- Properties (points, scalars) were stored separately in "universes"
- Constraints could operate on any properties
- Everything was "omniscient"—any part could see any other part

This is remarkably close to a modern entity-component system, implemented in 1963.

### The Limitation of Virtual Dispatch

A crucial observation about virtual functions emerges from Sketchpad's architecture: they only work well for simple, non-interacting systems. Sutherland could use virtual dispatch for drawing because rendering in Sketchpad didn't require interaction between different entity types—each shape drew itself independently.

But this breaks down for complex systems:
- Hidden line removal requires shapes to know about each other
- Physics simulation requires objects to interact
- Constraint solving requires cross-entity visibility

This is why the "omniscient" ring structure was essential. Virtual dispatch provides clean separation when you want isolation, but real-world systems often require the opposite: comprehensive visibility to solve hard problems. The compile-time hierarchy approach doubles down on isolation exactly where you need integration.

## The Path Not Taken: Why We Lost 35 Years

### The Misinterpretation

When Alan Kay saw Sketchpad, he focused on the virtual function dispatch (the function pointers), not the ring structure. He explicitly considered the "omniscient" aspect—where solvers could see all properties—as a weakness to be avoided. His biological background (molecular biology degree) predisposed him to think in terms of encapsulated cells passing messages, not systems with shared visibility.

This misunderstanding is crystallized in a quote from Alan Kay himself about ThingLab, a system that attempted to improve on Sketchpad:

> "ThingLab is an attempt to go beyond Sketchpad. Alan Borning's ThingLab pioneered a nice approach for dealing with constraints that did not require the solver to be omniscient."

Kay explicitly viewed Sketchpad's "omniscient" constraint solver—its ability to see and operate on all properties—as a weakness to be overcome. But this omniscience was precisely what made Sketchpad powerful. It allowed constraints to operate across any combination of properties without artificial barriers. The "problem" Kay identified was actually the solution.

Similarly, Stroustrup's distributed systems background made him think in terms of protected, isolated entities. Both saw encapsulation as fundamental, missing that Sketchpad's power came from the opposite: strategic lack of encapsulation at the entity level, with boundaries drawn around systems instead.

### The Propagation Problem

Ideas spread through champions who do the implementation work. Stroustrup implemented his interpretation into C++, which became wildly successful. The compile-time hierarchy model spread not because it was optimal, but because:
1. It was implemented in a popular language
2. It seemed intuitively appealing (matching our linguistic categorization habits)
3. It came bundled with genuinely useful features (type checking, which C badly needed)
4. Alternative approaches weren't championed with the same vigor

## The Alternative Architecture: Entity-Component Systems

### Looking Glass's Journey

Looking Glass Studios' path to ECS came through painful evolution:

1. **Ultima Underworld (early 1990s)**: Fat structs with overlapping fields (fuel and hit points shared memory)
2. **System Shock**: Single "outlink" pointer per entity to avoid overlaps
3. **Flight Unlimited**: Memory fragmentation forced game restarts ("out of fuel" meant memory too fragmented)
4. **Thief (1998)**: Full entity-component system

The key insight, attributed to Tom Leonard: instead of `entity.getHitPoints()`, use `hitPoints.get(entity)`. This inversion meant:
- Systems owned the data
- Type safety without interface explosion  
- Designers could compose entities from components
- Fast paths for physics and rendering

### The Architecture Pattern

ECS draws encapsulation boundaries around systems, not entities:
- Physics system owns all physics data
- Rendering system owns all rendering data
- Combat system owns all combat data

Entities become just IDs that index into these systems. This is still object-oriented in many senses—it just places the boundaries differently.

## Lessons for Modern Development

### On Architecture and Boundaries

The fundamental challenge in architecture is placing encapsulation boundaries correctly. Place them wrong, and you fight the architecture constantly. Place them right, and you save orders of magnitude of effort. The placement depends entirely on the problem domain—there's no universal answer.

### On Solving Hard Problems

Architecture should be driven by the hardest problems you face. Sketchpad solved constraint satisfaction—one of the hardest problems in UI development. That drove its architecture toward the ring structure. Modern architects should:
1. Identify the truly hard problems in their domain
2. Design architectures that make those problems tractable
3. Let simpler problems adapt to that architecture

### On the Persistence of Ideas

The compile-time hierarchy model persists in education despite its limitations. The top search results for "OOP tutorial" still teach vehicle hierarchies and shape examples—the exact patterns criticized in Stroustrup's own 1991 writings as problematic.

Breaking this cycle requires:
- Historical awareness of where ideas originated
- Critical examination of accepted patterns
- Willingness to look at successful systems that violate conventional wisdom
- Recognition that many "best practices" arose from constraints that no longer exist

## Conclusion: The Value of Historical Analysis

This historical journey reveals that many fundamental assumptions in software architecture arose not from careful analysis or empirical study, but from:
- Individual programmers solving specific problems with the tools available
- Misinterpretation of successful systems
- Ideas propagating based on implementation effort rather than merit
- Historical accidents and personal backgrounds of influential developers

The 35-year gap between Sketchpad's ring structure and Looking Glass's entity-component system represents lost opportunity—architectural patterns that could have advanced game development, tool design, and system architecture were overlooked because the wrong lessons were drawn from successful systems.

Understanding this history isn't just academic. It suggests that:
1. Many accepted "best practices" may be historical accidents
2. Successful patterns may exist in old systems, overlooked or misunderstood
3. The hardest problems should drive architecture, not textbook examples
4. Encapsulation boundaries are tools to be placed strategically, not dogma to follow blindly

The "Big OOPs" wasn't that object-oriented programming is wrong—it's that we spent 35 years teaching and implementing a specific pattern (compile-time hierarchies matching domain models) that arose from historical accident rather than architectural merit. The real power lies not in matching our mental models in code, but in organizing code to solve the actual computational problems at hand.
