# The Magic of Software: Domain-Oriented Development and the Future of Programming

*Based on Charles Simonyi's keynote at MODELS 2013*

## Introduction: Software as Complexity Absorption

After 50 years in the software industry, Charles Simonyi observes that software's fundamental magic lies in its unique ability to absorb complexity. Software represents "200 proof distilled complexity" - it captures models of the world (whether small or large scale) and manages the enormous variety inherent in real-world systems. This capacity for complexity absorption has enabled software to transform virtually every domain it touches.

## The Foundation: Why Moore's Law Works for Software

The exponential growth described by Moore's Law rests on two critical properties that make software uniquely scalable:

1. **Value invariance with physical size**: The number 42 has identical value whether displayed on a stadium screen or as a tiny footnote on a mobile device. Unlike physical goods where size correlates with value (a large Coke costs more than a small one), digital information maintains constant value regardless of physical representation.

2. **Speed increases with miniaturization**: As components shrink, signals travel shorter distances at constant speed (speed of light), enabling higher frequencies. Simonyi uses the analogy of an eagle's slow wing flaps versus a flea's rapid movements - both wing tips achieve similar airspeeds, but at vastly different frequencies.

These properties enable the continuous improvement cycle where smaller components become both cheaper to produce and faster to operate while maintaining full functionality.

## Digital Artifacts: The Transformation of Physical Complexity

Simonyi introduces the concept of "digital artifacts" - physical devices infested with software. These artifacts demonstrate a fundamental architectural pattern:

- **Physical layer**: Minimal sensors and actuators
- **Cyberspace layer**: Models and control logic in software
- **Interface**: The boundary where physical reality meets digital control

### Case Study: The Printer Evolution

The transformation from mechanical teletypewriter to modern inkjet printer illustrates this principle dramatically:

**Teletypewriter (mechanical era)**:
- Complex mechanical linkages for character positioning
- Physical constraints dictated design (carriage return + line feed as separate operations)
- Intricate mechanical timing mechanisms
- High cost, limited capability

**Modern inkjet printer**:
- 100x cheaper in constant dollars
- 100x more capable (graphics, color, multiple functions)
- 100x more reliable
- Complexity moved entirely to software models
- Generic, reusable hardware components (stepping motors, encoders, print heads)

The print heads that spray ink are so generic they were repurposed for DNA sequencing experiments, demonstrating how software-defined functionality enables hardware reuse across domains.

## The Software Engineering Era: Patterns and Problems

### Historical Context

The software engineering discipline emerged in the 1960s, crystallizing at the 1969 NATO conference. This era established patterns of practice that persist today:

- Separation between domain experts and programmers
- Focus on representing the solution (program) rather than the problem (domain)
- Code that looks remarkably similar to what we write today (Simonyi shows 1963 code that remains readable)

### The Cost Equation Problem

Traditional software engineering faces a multiplicative cost structure:

**Cost = E × A**

Where:
- E = number of entities in the implementation
- A = number of aspects (cross-cutting concerns)

Adding a new entity requires implementing all aspects for that entity. Adding a new aspect requires modifying all entities. This multiplicative relationship makes large systems exponentially expensive to build and maintain.

### Fred Brooks' Pessimism

Simonyi critiques Fred Brooks' famous assertion that programmers must master the "arbitrary complexity forced without rhyme or reason" by human institutions. Two fundamental issues with this view:

1. **The "must" assumption**: Why should programmers master domain complexity rather than domain experts?
2. **The dismissive attitude**: Calling customer requirements "without rhyme or reason" disrespects the people paying for the software

This perspective treats customers as obstacles rather than recognizing that their domain complexity has legitimate reasons and structure.

## The Diamond and Dirt Metaphor

Simonyi presents programming as "the opposite of diamond mining":

- **Diamond mining**: Extract valuable diamonds from dirt
- **Traditional programming**: Start with a diamond (clear intentions and requirements), then bury it under implementation dirt. All subsequent reasoning and modification must happen in the dirt.

This metaphor captures how implementation details obscure the original problem structure, making systems harder to understand and modify.

## Domain-Oriented Development: The Solution

### Core Principles

Domain-oriented development inverts traditional software engineering by focusing on the problem rather than the solution:

1. **Domain models are not executable** - they describe problems, not solutions
2. **Generators create executable code from domain descriptions**
3. **Domain experts directly participate in system specification**
4. **Changes happen at the domain level, regenerating implementation automatically**

### The New Cost Equation

Domain-oriented development transforms the cost structure to:

**Cost = E + A**

Aspects become additional generator processing steps that automatically distribute across entities. This linear relationship dramatically reduces costs for complex systems.

### The Scrambled Eggs Analogy

To illustrate when complexity is "real" versus artificial:

- **Looking at scrambled eggs alone**: Appears irreducibly complex
- **Too salty?** In traditional approach: Remove salt crystals with tweezers
- **With process knowledge**: Simply remake with less salt

When you maintain the recipe (domain model) alongside the result (implementation), fixing problems becomes trivial.

## Key Technologies for Domain Orientation

### 1. Uniform Intentional Representation

- Uses unique identifiers rather than text
- Meaning remains invariant across domain evolution
- Enables clean multi-domain integration without naming conflicts
- Supports non-textual elements (images, data sets)
- Enables high-quality groupware implementation
- Allows new parameters to be added to domains without breaking existing models

### 2. Projectional Editing

- Separation of model storage from presentation
- Complete freedom in notation without parsing constraints
- Dynamic notation switching without state loss
- Move beyond "WYSIWYG" limitations to "What You See Is What You Need Right Now"

Key insight: Domain experts want their familiar notations, which often aren't parseable. Projectional editing provides these notations while maintaining unambiguous internal representation.

### 3. Transformational Composition

Instead of imperative composition with intertwined control flow, transformational composition organizes programs as:

- Explicit state representations at each transformation stage
- Transformations as functions from state to state
- States expressed as domain-specific languages
- Aspects become predicates on state rather than control flow intrusions

### Relationship to Aspect-Oriented Programming

The transformational composition approach provides an elegant solution to problems that aspect-oriented programming (AOP) attempted to address. Simonyi acknowledges his colleague Gregor Kiczales' work on aspects - cross-cutting concerns that must be spread across a codebase.

Traditional AOP solutions like AspectJ extend programming languages with constructs called "point cuts" that describe where in the control flow an aspect should intervene. This requires programmers to:
- Master complex point cut languages
- Reason about aspect interactions
- Debug non-local control flow modifications

Simonyi characterizes aspects as "come from statements" (a play on "go to statements") because they specify where they should be called from, adding another layer of control flow complexity.

However, when using generative programming, aspects become part of the generation process rather than runtime control flow modifications. The generator can systematically apply aspects across all relevant entities without special language constructs.

In transformational composition specifically, aspects transform from control flow predicates to state predicates. Instead of asking "where in the code should this aspect execute?", we ask "what state conditions trigger this transformation?". This unifies aspect processing with regular transformations - both are simply functions from state to state, eliminating the artificial distinction between core logic and cross-cutting concerns.

## The Intentional Software Implementation

Simonyi's company, Intentional Software, has implemented these concepts in a concrete system called the Intentional Knowledge Workbench. Their approach demonstrates how theoretical domain-oriented principles translate to practical tooling.

### Core Architecture

The workbench operates as a quasi-general-purpose platform that can handle multiple domains simultaneously, rather than requiring separate tools for each domain. The system integrates the three foundational technologies described above:

1. **Uniform Intentional Representation**: All domain knowledge is stored in a database using unique identifiers. This allows meaning to remain invariant as domains evolve, clean mixing of multiple domains without naming conflicts, and integration of non-textual elements alongside traditional code.

2. **Projectional Multi-View Editing**: The editor is organized as a pure function that projects from the database to the screen, with all state (both UI and domain) maintained externally. This enables instant notation switching without losing any state or context, matching any traditional notation to keep domain experts comfortable, and seamless editing across domain boundaries.

3. **Transformational Composition**: Programs are structured as chains of transformations where each transformation converts from one DSL to another, moving from abstract (domain) to concrete (implementation). For editing, transformations must be bidirectional (screen output one direction, user input the reverse).

### Key Design Decisions

The system deliberately separates concerns:
- **Domain experts** create problem descriptions in familiar notations
- **Programmers** write generators (giving "the spark of life" to domain descriptions)  
- **The workbench** handles the complexity of multi-domain integration, notation management, and transformation chaining

Importantly, domain code is intentionally **not executable** - it simply states facts about the domain. This prevents the "user programming" trap where imperative features pollute domain languages, reintroducing complexity.

### Practical Applications Demonstrated

Intentional Software has applied their workbench to diverse domains:

- **Healthcare workbench**: CCE experts input health maintenance rules; system generates patient monitoring applications
- **Resource/task matching**: Inference rules expressed in domain-specific notation
- **Financial reporting**: Highly customized reports from domain specifications
- **Manufacturing**: Matching designs to factory capabilities
- **Dutch retirement planning**: Complex legislation encoded by domain experts ("Excel for legislators")
- **Aircraft maintenance**: Generating multiple coordinated outputs:
  - Procedural documentation
  - Quality assurance checklists
  - Equipment procurement lists
  - Parts requirements
  - Associated training materials

### Implementation Insights

The workbench addresses critical challenges:

**Multi-domain integration**: Every real system involves multiple domains (problem domain, UI, communication, etc.). The uniform representation allows clean composition without conflicts between domain notations.

**Domain evolution**: As domains grow and change, new parameters can be added freely. The projection system adjusts the "syntax" to accommodate new parameters while potentially hiding obsolete ones.

**Notation flexibility**: Domain experts can use their preferred notations (even if ambiguous or unparseable). The projection system handles this like natural conversation - when ambiguity arises, simply switch notation temporarily for clarification, just as humans rephrase statements for clarity.

**Beyond WYSIWYG**: Simonyi notes that WYSIWYG has become limiting - "what you see is *only* what you get." The projectional approach allows information to exist "behind" the current view, accessible through rapid notation switching. Users always see the notation most convenient for their current activity.

This multi-output, multi-domain capability demonstrates that domain models capture comprehensive domain knowledge reusable across many purposes - not just software requirements but the full understanding of a domain that can generate documentation, procedures, analyses, and executable systems.

## Practical Challenges and Solutions

### Why Isn't This Approach Universal?

Several factors impede adoption:

1. **Imperative tradition**: Developers emphasize executable code over declarative models
2. **"User programming" trap**: Adding imperative features to domain languages reintroduces complexity
3. **Notation challenges**: Traditional parseable syntax versus domain-natural notation
4. **Multi-domain reality**: Real problems span multiple domains
5. **Domain evolution**: Languages must evolve without breaking legacy models

## Simonyi's Law (Wish)

The ultimate goal: Reduce incremental development cost to **E_domain + A_domain**, approaching zero implementation cost asymptotically.

This parallels how compiler writing, once consuming 50-70% of programming effort, now represents a vanishing fraction of industry work. The vision is for implementation programming to similarly fade into infrastructure.

## Key Takeaways for Practitioners

1. **Resist executable models**: Keep domain models declarative and problem-focused
2. **Embrace projectional editing**: Free notation from parsing constraints
3. **Think transformationally**: Organize programs as state transformations, not control flow
4. **Involve domain experts directly**: They should own and modify domain models
5. **Separate concerns properly**: Let domain experts handle domain complexity, programmers handle generation
6. **Value patience**: Industry transformation takes decades, but mathematics favors this approach
7. **Consider aspects as state predicates**: Unify aspect handling with regular transformations
8. **Build for multiple outputs**: Domain models should generate more than just code

## Philosophical Insights

### On Software's Role
Software uniquely absorbs complexity from physical systems, enabling simpler, more reliable, and more capable artifacts. The question isn't whether to use software, but how to structure it for maximum effectiveness.

### On Problem vs. Solution
The least tractable representation of a problem is its implementation. Domain representations have fewer degrees of freedom and align with how experts think about problems.

### On Evolution
Just as mechanical computers gave way to electronic ones, and assembly language to high-level languages, implementation-focused programming will eventually yield to domain-oriented approaches. The math is inexorable: E + A beats E × A at scale.

### On Expertise
The attempt to make programmers master all domain complexity is both inefficient and disrespectful to domain expertise. Better to create tools that let domain experts express their knowledge directly.

## Conclusion

The future of software lies not in better implementation techniques but in eliminating implementation as a bottleneck. By focusing on domain models, projectional editing, and transformational composition, we can achieve Simonyi's vision where software development cost approaches the irreducible minimum: understanding and expressing what we want the system to do, not how to do it.

The modeling community is "on the right track" - the mathematics, the practical results, and the historical trajectory all point toward domain orientation as the future of software development. What's needed is patience, persistence, and continued development of the supporting tools and techniques. The Intentional Software workbench demonstrates that these ideas are not merely theoretical but can be implemented as practical, working systems that deliver real value across diverse domains.
