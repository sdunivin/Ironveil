# Git setup for Ironveil (Windows)

Git keeps every version of your project so you can undo any mistake, including changes Claude Code makes. Git LFS (Large File Storage) handles the big binary files: Unreal assets, Blender files, textures and sounds.

The GitHub repository already contains the Unreal project, `.gitignore`, `.gitattributes`, this guide and the README. You only need to clone it and open it.

## 1. Install the tools (once per computer)

1. Install **Git for Windows** from git-scm.com. Keep the default options; it includes Git LFS and the bash shell Claude Code needs.
2. Install **GitHub Desktop** from desktop.github.com and sign in with your GitHub account.
3. Open **Git Bash** (installed with Git for Windows) and run this once:

   ```bash
   git lfs install
   ```

## 2. Download (clone) the repository

1. In GitHub Desktop: **File → Clone repository**.
2. Choose **ironveil** from your list.
3. Set **Local path** to `C:\Users\YourName\Documents\Unreal Projects\ironveil` and click **Clone**.

## 3. Open the Unreal project

The repository already contains the Unreal project (`Ironveil.uproject`, `Config/` and `Source/`), so you don't need to create one.

1. In File Explorer, open your cloned `ironveil` folder. Right-click `Ironveil.uproject` → **Generate Visual Studio project files** (on Windows 11, **Show more options** first).
2. Double-click `Ironveil.uproject`. If Unreal asks to rebuild missing modules, click **Yes**.
3. Continue with [Docs/PHASE1-EDITOR-SETUP.md](Docs/PHASE1-EDITOR-SETUP.md).

> Windows hides files that start with a dot in some views. In File Explorer, turn on **View → Show → Hidden items** to see `.gitignore` and `.gitattributes`.

## 4. Commit and push

1. In GitHub Desktop, you'll see your project files listed as changes.
2. Type a summary, e.g. `Phase 1 editor setup`.
3. Click **Commit to main**, then **Push origin**.

## 5. Check that LFS is working

In Git Bash, inside the project folder:

```bash
git lfs ls-files
```

You should see your `.uasset` and `.umap` files listed. If you have assets but the list is empty, stop and ask Claude for help before going further.

## 6. Daily habit

At the end of every work session, and before letting Claude Code change anything:

1. Open GitHub Desktop.
2. Write a short summary of what you did ("Added dodge with i-frames").
3. Click **Commit to main**, then **Push origin**.

## Storage note

Unreal projects grow quickly. GitHub's free Git LFS storage and bandwidth are limited, so check GitHub's current LFS quota in your account's billing settings. When you outgrow it, you can buy more LFS data on GitHub or move to a host built for large game projects (for example Azure DevOps, a self-hosted Gitea server, Perforce or Diversion). Ask Claude to compare current options when you get there.
