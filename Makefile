all:
	./rebar compile

bench:
	erl -pa ebin -noshell -s jtstat bench -s init stop

global_bench:
	erl -pa ebin -noshell -s jtstat global_bench -s init stop
