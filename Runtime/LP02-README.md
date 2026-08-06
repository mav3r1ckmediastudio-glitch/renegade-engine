# LP02 Runtime Story Flow proof

The packaged Runtime fixture demonstrates the serialized minimum flow:

`Game Start -> Level One -> Level Two -> Complete Game`

Run `Run-LP02-Flow-Proof.cmd` beside `RenegadeRuntime.exe`. The launcher selects
the LP02 project and supplies two diagnostic `level.complete` outcomes. Runtime
first emits its engine-owned `renegade.flow.start` outcome, loads Level One,
then consumes the two supplied outcomes to load Level Two and reach Complete
Game.

Inspect `Logs/RuntimeBootstrap.log` after startup. A passing record contains:

- `status=PASS`
- `code=SUCCESS`
- the expected `startup_flow_id`
- `flow_node_name=Complete Game`
- `flow_terminal=complete_game`
- `flow_trace_count=4`

The two WISCENEs use adjacent `.rmeta` identity envelopes. They are copied from
the Wicked cube fixture only to provide visible packaged scenes; no flow
semantics live in those scenes.
