#!/usr/bin/env bash
set -euo pipefail

# Configure Git bot identity
git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'

echo "==> 1. Fetching origin branches and upstream ezEngine..."
git fetch --prune origin '+refs/heads/*:refs/remotes/origin/*'
git fetch https://github.com/ezEngine/ezEngine.git 'refs/heads/dev:refs/remotes/upstream/dev'

# Switch to cutecore-dev
git checkout -B cutecore-dev origin/cutecore-dev

# Merge upstream dev into cutecore-dev
echo "==> 2. Merging upstream ezEngine/dev into cutecore-dev..."
if ! git merge --no-edit -m "Sync: Merge upstream ezEngine/dev" upstream/dev; then
  echo "::error::Merge conflict with upstream ezEngine:dev. Please resolve manually."
  git merge --abort
  exit 1
fi

# Discover all plugin branches (origin/plugin/*)
mapfile -t plugin_branches < <(git for-each-ref --sort=refname --format='%(refname:short)' refs/remotes/origin/plugin/)

if (( ${#plugin_branches[@]} == 0 )); then
  echo "::warning::No plugin branches found matching 'origin/plugin/*'."
fi

merged_plugins=()
failed_plugins=()

# Sequentially merge each plugin branch
echo "==> 3. Merging plugin branches..."
for branch in "${plugin_branches[@]}"; do
  plugin_name="${branch#origin/}"
  echo "-> Merging plugin: $plugin_name ($branch)..."
  
  if git merge --no-edit -m "Integrate plugin: $plugin_name" "$branch"; then
    merged_plugins+=("$plugin_name")
  else
    echo "::error::Merge conflict in plugin '$plugin_name'!"
    git diff --name-only --diff-filter=U
    git merge --abort
    failed_plugins+=("$plugin_name")
  fi
done

# Abort push if any plugin failed to merge
if (( ${#failed_plugins[@]} > 0 )); then
  echo "::error::Sync aborted due to merge conflicts in: ${failed_plugins[*]}"
  exit 1
fi

# Push only the integrated cutecore-dev branch
echo "==> 4. Pushing updated cutecore-dev..."
git push origin cutecore-dev

# Generate GitHub Actions step summary
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  {
    echo "## 🚀 Cutecore Superfork Synced Successfully"
    echo "- **Upstream ezEngine dev commit:** \`$(git rev-parse upstream/dev)\`"
    echo "- **Integrated cutecore-dev commit:** \`$(git rev-parse HEAD)\`"
    echo ""
    echo "### 🧩 Merged Plugins (${#merged_plugins[@]}):"
    for p in "${merged_plugins[@]}"; do
      echo "- \`$p\`"
    done
  } >> "$GITHUB_STEP_SUMMARY"
fi
