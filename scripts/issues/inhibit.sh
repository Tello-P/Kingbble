# Keeps the machine awake for a long run (source it first, don't run it): the
# sourcing script re-executes itself under systemd-inhibit (sleep, idle and lid
# switch blocked) when it is available. KINGBBLE_INHIBITED marks a script that is
# already covered, so the scripts it starts do not inhibit again; dry runs skip it.
if [[ -z ${KINGBBLE_INHIBITED:-} && " $* " != *" --dry-run "* ]] &&
  command -v systemd-inhibit >/dev/null; then
  export KINGBBLE_INHIBITED=1
  exec systemd-inhibit --what=sleep:idle:handle-lid-switch --who=kingbble \
    --why="$(basename "$0") $*" --mode=block bash "$0" "$@"
fi
