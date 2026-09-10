#!/usr/bin/env bash
set -euo pipefail

git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git fetch --prune origin '+refs/heads/*:refs/remotes/origin/*'
git fetch https://github.com/ezEngine/ezEngine.git 'refs/heads/dev:refs/remotes/cutecore-upstream/dev'

# Refuse to rewrite dev if it has diverged from upstream.
git merge-base --is-ancestor origin/dev refs/remotes/cutecore-upstream/dev
git switch --force-create cutecore-sync-dev origin/dev
git merge --ff-only refs/remotes/cutecore-upstream/dev
git switch --force-create cutecore-integration origin/cutecore-dev

merge_branch() {
  if ! git merge --no-edit "$1"; then
    echo "::error::Merge conflict with $1. No branches have been pushed. Resolve locally and push the resolution to cutecore-dev."
    git diff --name-only --diff-filter=U
    git merge --abort
    exit 1
  fi
}

merge_branch cutecore-sync-dev
# Only this namespace is publishable. WIP and backup branches are excluded.
mapfile -t plugins < <(git for-each-ref --sort=refname --format='%(refname)' refs/remotes/origin/plugin/)
if (( ${#plugins[@]} == 0 )); then
  echo '::error::No published plugin branches found; refusing to publish.'
  exit 1
fi
for plugin in "${plugins[@]}"; do
  merge_branch "$plugin"
done

# Protect incomplete plugins even if they accidentally reach a public input.
for path in Code/EnginePlugins/AudioEchoPlugin Code/UnitTests/AudioEchoPluginTest Code/EditorPlugins/GreyBoxExtended Code/EnginePlugins/GreyBoxPlugin Code/UnitTests/EditorPluginGreyBoxTest; do
  if [[ -n "$(git ls-tree -r --name-only HEAD -- "$path")" ]]; then
    echo "::error::Unfinished plugin found: $path. No branches have been pushed."
    exit 1
  fi
done

# Concurrent human pushes are rejected normally. Never force-push either branch.
git push --atomic origin cutecore-sync-dev:refs/heads/dev HEAD:refs/heads/cutecore-dev
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  {
    echo '### Cutecore integration'
    echo "Upstream dev: $(git rev-parse cutecore-sync-dev)"
    echo "Integrated commit: $(git rev-parse HEAD)"
    printf '\nMerged plugin branches:\n'
    printf -- '- %s\n' "${plugins[@]}"
  } >> "$GITHUB_STEP_SUMMARY"
fi
