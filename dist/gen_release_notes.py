import subprocess
import sys

def get_commits(branch: str, start_tag: str, end_tag: str) -> list[str]:
    try:
        commits_raw = subprocess.check_output([ "git", "--no-pager", "log", branch, "--no-color", "--pretty=oneline", "--abbrev-commit", f"{start_tag}..{end_tag}"], stderr=subprocess.DEVNULL).decode("UTF-8").split("\n")
    except:
        return []

    commits = []
    for line in commits_raw:
        commits.append(line[9:])

    return commits

def main(args: list) -> int:
    if len(args) != 2:
        print(f"Usage: {args[0]} prev_minor")
        return 1
    
    last_minor_version = f"v1.{args[1]}"

    master_commits = get_commits("master", f"{last_minor_version}.0", "master")

    for i in range(1, 100):
        branch_commits = get_commits(f"releases/{last_minor_version}.X", f"{last_minor_version}.0", f"{last_minor_version}.{i}")

        if len(branch_commits) == 0:
            break

        master_commits = [commit for commit in master_commits if commit not in branch_commits]

    sorted_commits = {}
    for commit in master_commits:
        category, commit_name = commit.split(":", 1)

        if category not in sorted_commits:
            sorted_commits[category] = []
        sorted_commits[category].append(commit_name)

    for category in sorted_commits:
        print(f"## {category}\n")
        for commit in sorted_commits[category]:
            print(f"- {commit}")
        print(f"\n")

if __name__ == "__main__":
    exit(main(sys.argv))