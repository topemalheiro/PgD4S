# PlugData gotchas

## Clicking to bang / trigger

In PlugData / Pure Data:

- **Message boxes** — e.g. `hello world(` — can be clicked to output their contents.
- **Object boxes** — e.g. `[bang]`, `[print]`, `[+ 1]` — generally **cannot** be clicked to trigger them.
  - Exception: GUI objects like `[bang]`, `[toggle]`, `[slider]`, etc. are clickable because they are controls.

If you want to manually fire a message, either:
- Use a **message box** and click it, or
- Use a `[bang]` object connected to the inlet of the object you want to trigger.

Remember: clicking an object box won’t bang it — click the message box instead.
