# Route matching: path semantics

The route matcher (`include/matcher/route.hpp`) maps an HTTP method and a request path to a
route and the values its parameters capture. These are its rules.

## Patterns

A pattern starts with `/` and is split at every `/` into segments:

| Segment | Matches |
|---|---|
| `users` | the same bytes |
| `{name}` or `{name:string}` | one non-empty segment, any bytes except `/` |
| `{name:u64}` | one segment that is an unsigned 64-bit decimal number |
| `{name:i64}` | one segment that is a signed 64-bit decimal number |
| `{*name}` | the rest of the path, non-empty; only as the last segment |

A literal segment may contain only RFC 3986 pchar bytes (unreserved, sub-delims, `:`, `@`, and
`%` followed by two hex digits). A name is `[A-Za-z_][A-Za-z0-9_]*`, and a name appears once per
pattern. A pattern has at most 32 segments and 16 parameters. `parse_pattern` reports the
first error and its byte offset; the checked front end (`checked_pattern`) turns each error
into a compile-time error that names it.

## Matching

1. **Raw bytes.** Paths are compared and captured as sent. Nothing is percent-decoded; a
   handler that wants decoded values decodes them.
2. **Segments.** A parameter matches one segment of one or more bytes, so every pchar is
   accepted in a parameter, and an empty segment never matches a parameter.
3. **Catch-all.** `{*name}` matches a non-empty remainder of the path, taken as sent:
   `/f/{*p}` matches `/f/a` (`a`) and `/f/a/b/` (`a/b/`), but not `/f` or `/f/`.
4. **Trailing slash is strict.** `/a/b` and `/a/b/` are different paths; neither matches the
   other's route, and the matcher never redirects.
5. **Precedence.** At every segment, left to right: a literal beats a typed parameter, which
   beats a plain parameter, which beats a catch-all. The most specific route wins, whatever
   order the routes were registered in, so a fully literal route beats any other route that
   matches the same path.
6. **Methods and 405.** The matcher first finds the most specific route of the request's
   method. If there is none, it answers "method not allowed" with the set of methods whose
   routes match the path, and "not found" when no method's route does.
7. **Duplicates are errors.** Two routes of one method that match exactly the same set of
   paths (the same segments and kinds, whatever the parameter names) are rejected: at compile
   time in a compile-time table, by an error of `build_table` at run time.

A server built on the matcher decides what HEAD and OPTIONS mean; the matcher answers for the
method it is asked.

## What a lookup returns

The route's value and up to 16 captured values, as views into the path given to the lookup.
A lookup allocates nothing. A typed parameter is checked during the match, because its type
takes part in routing (`/users/{id:u64}` does not match `/users/me`); it is converted to a
number only when the checked front end calls a handler.
