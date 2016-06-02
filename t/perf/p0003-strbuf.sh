#!/bin/sh

test_description='Benchmark different uses of strbuf'
. ./perf-lib.sh

test_perf_default_repo

for variant in $(seq 0 4)
do
	export variant
	test_perf "variant $variant, 5 short strings" '
		test-strbuf-perf $variant "short" 5
	'
done

for variant in $(seq 0 4)
do
	export variant
	test_perf "variant $variant, 20 short strings" '
		test-strbuf-perf $variant "short" 20
	'
done

for variant in $(seq 0 4)
do
	export variant
	test_perf "variant $variant, 500 short strings" '
		test-strbuf-perf $variant "short" 500
	'
done

for variant in $(seq 0 4)
do
	export variant
	test_perf "variant $variant, 5 long strings" '
		test-strbuf-perf $variant "this is a string that we will repeatedly insert" 5
	'
done

for variant in $(seq 0 4)
do
	export variant
	test_perf "variant $variant, 20 long strings" '
		test-strbuf-perf $variant "this is a string that we will repeatedly insert" 20
	'
done

for variant in $(seq 0 4)
do
	export variant
	test_perf "variant $variant, 500 long strings" '
		test-strbuf-perf $variant "this is a string that we will repeatedly insert" 500
	'
done

test_done
