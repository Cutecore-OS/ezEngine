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
  # Auto-resolve .github conflicts if upstream doesn't have them
  mapfile -t upstream_conflicts < <(git diff --name-only --diff-filter=U)
  real_upstream_conflicts=()
  for file in "${upstream_conflicts[@]}"; do
    if [[ "$file" != .github/* ]]; then
      real_upstream_conflicts+=("$file")
    fi
  done

  if (( ${#real_upstream_conflicts[@]} == 0 )) && (( ${#upstream_conflicts[@]} > 0 )); then
    echo "-> Auto-resolving .github/ conflicts with upstream..."
    git checkout HEAD -- .github/
    git add .github/
    git commit --no-edit
  else
    echo "::error::Merge conflict with upstream ezEngine:dev. Please resolve manually."
    git merge --abort
    exit 1
  fi
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
  
  if ! git merge --no-edit -m "Integrate plugin: $plugin_name" "$branch"; then
    # Get all conflicted/unmerged files
    mapfile -t unmerged_files < <(git diff --name-only --diff-filter=U)
    
    # Filter out .github/ files
    real_conflicts=()
    for file in "${unmerged_files[@]}"; do
      if [[ "$file" != .github/* ]]; then
        real_conflicts+=("$file")
      fi
    done

    # Special handling for CMakePresets.json conflicts
    if [[ " ${unmerged_files[@]} " =~ " CMakePresets.json " ]]; then
      echo "-> Detected CMakePresets.json conflict in '$plugin_name', attempting to auto-resolve..."
      
      # Extract versions from both sides of the merge
      ours=$(git show :1:CMakePresets.json 2>/dev/null || echo "{}")  # Base
      theirs=$(git show :3:CMakePresets.json 2>/dev/null || echo "{}")  # Their version
      current=$(git show :2:CMakePresets.json 2>/dev/null || echo "{}")  # Our current
      
      # Use a Python-based merge strategy to intelligently combine JSON objects
      python3 << 'PYTHON_EOF'
import json
import sys

try:
  # Read the three versions (base, ours, theirs)
  with open('CMakePresets.json', 'r') as f:
    content = f.read()
    # Remove git conflict markers and extract the actual sections
    if '<<<<<<< HEAD' in content:
      ours_section = content.split('<<<<<<< HEAD')[1].split('=======')[0].strip()
      theirs_section = content.split('=======')[1].split('>>>>>>>')[0].strip()
    else:
      # No conflict markers found, read normally
      merged = json.loads(content)
      with open('CMakePresets.json', 'w') as out:
        json.dump(merged, out, indent=2)
      sys.exit(0)
  
  # Parse the two versions
  ours_data = json.loads(ours_section)
  theirs_data = json.loads(theirs_section)
  
  # Merge: take base and merge in all changes from both sides
  merged_data = ours_data.copy()
  
  # For vs2022x64 preset, merge the cacheVariables from both sides
  if 'configurePresets' in merged_data and 'configurePresets' in theirs_data:
    for preset_idx, preset in enumerate(merged_data['configurePresets']):
      if preset.get('name') == 'vs2022x64':
        # Find matching preset in theirs
        theirs_preset = None
        for tp in theirs_data.get('configurePresets', []):
          if tp.get('name') == 'vs2022x64':
            theirs_preset = tp
            break
        
        if theirs_preset and 'cacheVariables' in theirs_preset:
          # Merge cacheVariables from theirs into ours
          preset.setdefault('cacheVariables', {})
          for key, val in theirs_preset['cacheVariables'].items():
            if key not in preset['cacheVariables']:
              preset['cacheVariables'][key] = val
  
  # Write the merged result
  with open('CMakePresets.json', 'w') as f:
    json.dump(merged_data, f, indent=2)
    f.write('\n')
  
  print("✓ CMakePresets.json merged successfully")
except Exception as e:
  print(f"✗ Failed to auto-merge CMakePresets.json: {e}", file=sys.stderr)
  sys.exit(1)
PYTHON_EOF

      if [ $? -eq 0 ]; then
        git add CMakePresets.json
        # Remove CMakePresets.json from conflict list
        real_conflicts=("${real_conflicts[@]/CMakePresets.json/}")
        real_conflicts=( "${real_conflicts[@]}" )  # Clean up array
      else
        echo "::warning::Failed to auto-merge CMakePresets.json, keeping merge conflict marker"
      fi
    fi

    # If the ONLY conflicts are in .github/, keep HEAD's version and finish merge
    if (( ${#real_conflicts[@]} == 0 )) && (( ${#unmerged_files[@]} > 0 )); then
      echo "-> Auto-resolving .github/ conflicts for '$plugin_name' (keeping CI scripts)..."
      git checkout HEAD -- .github/
      git add .github/
      git commit --no-edit
      merged_plugins+=("$plugin_name")
    elif (( ${#real_conflicts[@]} == 0 )); then
      # All conflicts resolved
      git commit --no-edit
      merged_plugins+=("$plugin_name")
    else
      echo "::error::Real merge conflict in plugin '$plugin_name'!"
      printf '::error::Conflicted file: %s\n' "${real_conflicts[@]}"
      git merge --abort
      failed_plugins+=("$plugin_name")
    fi
  else
    merged_plugins+=("$plugin_name")
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
