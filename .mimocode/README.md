# MiMoCode Configuration for Rats Search

This directory contains MiMoCode agent and skill configurations for the Rats Search project.

## Structure

```
.mimocode/
├── config.json          # Project and agent configurations
├── settings.json        # Build, test, and code style settings
├── skills/             # Reusable skill definitions
│   ├── add-widget.md       # Add new Qt widget
│   ├── add-api-endpoint.md # Add REST API endpoint
│   ├── add-test.md         # Add unit test
│   ├── debug-fix.md        # Debug and fix issues
│   └── code-review.md      # Code review checklist
└── README.md           # This file
```

## Agents

### explore
Explore codebase structure, find files, and understand architecture.

### builder
Build and compile the project with CMake/Ninja.

### debugger
Debug issues, analyze errors, and fix bugs.

### reviewer
Review code changes and ensure quality.

## Skills

### add-widget
Add a new Qt widget to the project:
- Create header and source files
- Update CMakeLists.txt
- Follow existing patterns

### add-api-endpoint
Add a new REST API endpoint:
- Add method to RatsAPI
- Wire in ApiServer
- Update documentation

### add-test
Add a new unit test:
- Create test file with Qt Test
- Update tests/CMakeLists.txt
- Follow test conventions

### debug-fix
Debug and fix common issues:
- Build errors
- Runtime errors
- Memory issues
- GUI issues

### code-review
Review code changes:
- Code style compliance
- Memory management
- Error handling
- Performance
- Testing

## Usage

MiMoCode will automatically load these configurations when working on the Rats Search project. You can also reference these files for guidance on common tasks.

## Customization

To add new skills:
1. Create a new `.md` file in `skills/`
2. Follow the existing format (Description, Steps, Examples)
3. Reference the skill in `config.json` if needed

## References

- Project documentation: `AGENTS.md`
- Build instructions: `README.md`
- API documentation: `docs/API.md`
