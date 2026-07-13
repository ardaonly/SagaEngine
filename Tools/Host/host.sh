#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
compose_file="$script_dir/docker/docker-compose.yml"
project_name="${SAGA_HOST_PROJECT:-sagaengine-host}"

fail() {
    printf '[host] ERROR: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage:
  host.sh <command> [args]

Commands:
  start       Start the local dependency stack
  stop        Stop the dependency stack without deleting volumes
  restart     Restart the dependency stack
  logs        Show/follow Docker Compose logs
  status      Show Docker Compose service status
  help        Show this help

Environment:
  SAGA_HOST_PROJECT        Compose project name (default: sagaengine-host)
  SAGA_HOST_POSTGRES_PORT  Postgres host port (default: 5432)
  SAGA_HOST_REDIS_PORT     Redis host port (default: 6379)
EOF
}

require_compose() {
    command -v docker >/dev/null 2>&1 || fail "docker is not installed or not on PATH"

    if docker compose version >/dev/null 2>&1; then
        return 0
    fi

    if command -v docker-compose >/dev/null 2>&1; then
        return 0
    fi

    fail "Docker Compose is not available; install the docker compose plugin or docker-compose"
}

compose() {
    require_compose

    if docker compose version >/dev/null 2>&1; then
        docker compose --project-directory "$repo_root" --project-name "$project_name" -f "$compose_file" "$@"
    else
        docker-compose --project-directory "$repo_root" -p "$project_name" -f "$compose_file" "$@"
    fi
}

run_compose() {
    if ! compose "$@"; then
        fail "docker compose failed: $*"
    fi
}

command_name="${1:-help}"
if [ "$#" -gt 0 ]; then
    shift
fi

case "$command_name" in
    start|up)
        printf '[host] starting local dependency stack (%s)\n' "$project_name"
        run_compose up -d
        ;;
    stop)
        printf '[host] stopping local dependency stack (%s)\n' "$project_name"
        run_compose stop "$@"
        ;;
    restart)
        printf '[host] restarting local dependency stack (%s)\n' "$project_name"
        run_compose restart "$@"
        ;;
    logs)
        run_compose logs -f "$@"
        ;;
    status|ps)
        run_compose ps "$@"
        ;;
    help|--help|-h)
        usage
        ;;
    *)
        usage >&2
        fail "unknown command: $command_name"
        ;;
esac
