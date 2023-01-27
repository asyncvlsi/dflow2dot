# dflow2dot

This converts an ACT dataflow graph into a per-process `.dot` file suitable for visualization using graphviz. 

This requires [interact](https://github.com/asyncvlsi/interact) to be installed.

### Running dflow2dot
   $ dflow2dot -p <process> <file.act>

This should save a dot file per process that has an ACT dataflow body.
