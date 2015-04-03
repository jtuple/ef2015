-module(jtstat).
-compile(export_all).
-on_load(init/0).

-record(metric, {min,
                 median,
                 p90,
                 p95,
                 p99,
                 p999,
                 max}).

%%%===================================================================
%%% Global API
%%%===================================================================

-spec setup_metrics(non_neg_integer()) -> ok.
setup_metrics(_Num) ->
    erlang:nif_error({error, not_loaded}).

-spec report_random(non_neg_integer()) -> ok | error.
report_random(_Metric) ->
    erlang:nif_error({error, not_loaded}).

-spec report(non_neg_integer(), integer()) -> ok | error.
report(_Metric, _Value) ->
    erlang:nif_error({error, not_loaded}).

-spec query(non_neg_integer()) -> #metric{} | error.
query(_Metric) ->
    erlang:nif_error({error, not_loaded}).

%%%===================================================================
%%% Application namespaced API
%%%===================================================================

-spec setup_metrics(atom(), non_neg_integer()) -> ok.
setup_metrics(_App, _Num) ->
    erlang:nif_error({error, not_loaded}).

-spec report_random(atom(), non_neg_integer()) -> ok | error.
report_random(_App, _Metric) ->
    erlang:nif_error({error, not_loaded}).

-spec report(atom(), non_neg_integer(), integer()) -> ok | error.
report(_App, _Metric, _Value) ->
    erlang:nif_error({error, not_loaded}).

-spec query(atom(), non_neg_integer()) -> #metric{} | error.
query(_App, _Metric) ->
    erlang:nif_error({error, not_loaded}).

%%%===================================================================
%%% Internal
%%%===================================================================

init() ->
    case code:which(?MODULE) of
        Filename when is_list(Filename) ->
            NIF = filename:join([filename:dirname(Filename),"../priv", "jtstat"]),
            erlang:load_nif(NIF, 0)
    end.

%%%===================================================================
%%% Global Metric Benchmark
%%%===================================================================

global_bench() ->
    global_bench(8, 200, 1000000).

global_bench(Workers, Metrics, Range) ->
    jtstat:setup_metrics(Metrics),
    {Time, _} = timer:tc(fun() ->
                                 run_global_bench(Workers,
                                                  Metrics,
                                                  Range)
                         end),
    io:format("Duration: ~p ms~n", [Time div 1000]),
    io:format("Events/s: ~p~n", [(Metrics * Range) div (Time div 1000000)]),
    ok.

run_global_bench(Workers, Metrics, Range) ->
    Events = Range div Workers,
    Reporter = spawn(fun() ->
                             global_reporter(0, Metrics)
                     end),
    _ = [spawn_monitor(fun() ->
                               global_worker(N, Events, Metrics)
                       end) || N <- lists:seq(1, Workers)],
    _ = [receive _ -> ok end || _ <- lists:seq(1, Workers)],
    exit(Reporter, kill),
    ok.

global_reporter(Metrics, Metrics) ->
    receive
    after 1000 ->
            ok
    end,
    global_reporter(0, Metrics);
global_reporter(Metric, Metrics) ->
    io:format("~p~n", [query(Metric)]),
    global_reporter(Metric+1, Metrics).

global_worker(_Id, 0, _Metrics) ->
    ok;
global_worker(Id, Events, Metrics) ->
    global_worker_metrics(Id, Metrics),
    global_worker(Id, Events - 1, Metrics).

global_worker_metrics(_Id, 0) ->
    ok;
global_worker_metrics(Id, Metric) ->
    report_random(Metric - 1),
    global_worker_metrics(Id, Metric - 1).

%%%===================================================================
%%% Namespaced Metric Benchmark
%%%===================================================================

bench() ->
    bench(8, 200, 1000000).

bench(Workers, Metrics, Range) ->
    jtstat:setup_metrics(test, Metrics),
    {Time, _} = timer:tc(fun() ->
                                 run_bench(Workers, Metrics, Range)
                         end),
    io:format("Duration: ~p ms~n", [Time div 1000]),
    io:format("Events/s: ~p~n", [(Metrics * Range) div (Time div 1000000)]),
    ok.

run_bench(Workers, Metrics, Range) ->
    Events = Range div Workers,
    Reporter = spawn(fun() ->
                             reporter(0, Metrics)
                     end),
    _ = [spawn_monitor(fun() ->
                               worker(N, Events, Metrics)
                       end) || N <- lists:seq(1, Workers)],
    _ = [receive _ -> ok end || _ <- lists:seq(1, Workers)],
    exit(Reporter, kill),
    ok.

reporter(Metrics, Metrics) ->
    receive
    after 1000 ->
            ok
    end,
    reporter(0, Metrics);
reporter(Metric, Metrics) ->
    io:format("~p~n", [query(test, Metric)]),
    reporter(Metric+1, Metrics).

worker(_Id, 0, _Metrics) ->
    ok;
worker(Id, Events, Metrics) ->
    worker_metrics(Id, Metrics),
    worker(Id, Events - 1, Metrics).

worker_metrics(_Id, 0) ->
    ok;
worker_metrics(Id, Metric) ->
    report_random(test, Metric - 1),
    worker_metrics(Id, Metric - 1).
