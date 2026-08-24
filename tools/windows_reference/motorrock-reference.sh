#!/bin/zsh

set -euo pipefail

reference_vm="${MOTORROCK_REFERENCE_VM:-Windows 11}"
reference_game='\\Mac\Home\Downloads\Motor Rock\MR.exe'
reference_helper='C:\Users\Public\Documents\MotorRockReferenceControl.exe'
reference_csc='C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'

script_dir="${0:A:h}"
host_home="/Users/${USER}"
source_file="${script_dir}/MotorRockReferenceControl.cs"
source_relative="${source_file#${host_home}}"
source_windows="\\\\Mac\\Home${source_relative//\//\\}"

usage() {
  print -u2 'Usage: motorrock-reference.sh COMMAND [ARGS]'
  print -u2 '  build'
  print -u2 '  start'
  print -u2 '  status'
  print -u2 '  focus'
  print -u2 '  move X Y'
  print -u2 '  click X Y'
  print -u2 '  key NAME [COUNT]'
  print -u2 '  game-capture OUTPUT.png'
  print -u2 '  capture OUTPUT.png'
  print -u2 '  guest-capture WINDOWS_OUTPUT.png'
}

require_args() {
  local required="$1"
  shift
  if (( $# < required )); then
    usage
    exit 2
  fi
}

build_helper() {
  prlctl exec "${reference_vm}" --current-user "${reference_csc}" \
    /nologo \
    /target:exe \
    "/out:${reference_helper}" \
    /reference:System.Drawing.dll \
    "${source_windows}"
}

run_helper() {
  prlctl exec "${reference_vm}" --current-user "${reference_helper}" "$@"
}

host_path_to_windows() {
  local host_path="${1:A}"
  if [[ "${host_path}" != "${host_home}"/* ]]; then
    print -u2 -r -- "Game capture output must be inside ${host_home}: ${host_path}"
    return 2
  fi
  local relative_path="${host_path#${host_home}}"
  print -r -- "\\\\Mac\\Home${relative_path//\//\\}"
}

command_name="${1:-}"
if [[ -z "${command_name}" ]]; then
  usage
  exit 2
fi
shift

case "${command_name}" in
  build)
    build_helper
    ;;
  start)
    prlctl status "${reference_vm}" | grep -q running || prlctl start "${reference_vm}"
    build_helper
    run_helper start "${reference_game}"
    print -r -- "Started ${reference_game} in ${reference_vm}"
    ;;
  status)
    run_helper status
    ;;
  focus)
    run_helper focus
    ;;
  move)
    require_args 2 "$@"
    run_helper move "$1" "$2"
    ;;
  click)
    require_args 2 "$@"
    run_helper click "$1" "$2"
    ;;
  key)
    require_args 1 "$@"
    run_helper key "$@"
    ;;
  game-capture)
    require_args 1 "$@"
    windows_output="$(host_path_to_windows "$1")"
    run_helper screenshot "${windows_output}"
    print -r -- "The game screen is stored in ${1:A}."
    ;;
  capture)
    require_args 1 "$@"
    prlctl capture "${reference_vm}" --file "${1:A}"
    ;;
  guest-capture)
    require_args 1 "$@"
    run_helper screenshot "$1"
    ;;
  *)
    usage
    exit 2
    ;;
esac
