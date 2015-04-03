## jtstat : simple NIF-based histogram library for Erlang (prototype)

This branch contains prototype code for a NIF-based histogram stats
metric system for Erlang. The prototype is hardcoded to compute 1-second
resolution histograms.

### Global API

The global metric API assumes there is a fixed number of metrics identified
by number. These could in practice be named via preprocessor magic.

Example:

```erlang
-define(STAT_FOO, 1).
-define(STAT_BAR, 2).
-define(STAT_BAZ, 3).
-define(STATS, 3).

init() ->
    jtstat:setup_metrics(?STATS).

report_foo(Val) ->
    jtstat:report(?STAT_FOO, Val).

query_foo() ->
    jtstat:query(?STAT_FOO).
```

Because stats are identified by name, lookup is quick.

```
make
erl -pa ebin

> jtstat:global_bench().

<snip>

Duration: 6646 ms
Events/s: 33333333
```

### Application namespaced API

Of course, global numeric stats are impossible in real world applications as
it becomes a nightmare to compose different libraries/applications that each
have their own stats.

Thus, this prototype also supports namespaced stats, where the namespace is
an atom and is expected to be an Erlang application name by convention.


```erlang
-define(MYAPP_STAT_FOO, 1).
-define(MYAPP_STAT_BAR, 2).
-define(MYAPP_STAT_BAZ, 3).
-define(MYAPP_STATS, 3).

init() ->
    jtstat:setup_metrics(myapp, ?MYAPP_STATS).

report_foo(Val) ->
    jtstat:report(myapp, ?MYAPP_STAT_FOO, Val).

query_foo() ->
    jtstat:query(myapp, ?MYAPP_STAT_FOO).
```

The extra lookup indirection hurts raw performance, but the metric system
is still very fast compared to things like folsom / exometer.

```
make
erl -pa ebin

> jtstat:bench().

<snip>

Duration: 9785 ms
Events/s: 22222222
```

## Histogram result

The record returned by the `jtstat:query` functions is as follows:

```erlang
-record(metric, {min,
                 median,
                 p90,
                 p95,
                 p99,
                 p999,
                 max}).
```
