🌳 C-Gitlet 🌳
A simple version control system inspired by Git, written in pure C. 📜
🤔 How to Use 🤔

Compile the code: 🔨
gcc gitlet.c -o gitlet


Initialize a repository: 🚀
./gitlet init

Or for a bare repository (for remotes): 🗄️
./gitlet init --bare


Add files to the index: ➕
./gitlet add <file>


Commit your changes: ✅
./gitlet commit "<message>"


View commit history: 📜
./gitlet log


Check status: 🕵️
./gitlet status


Create or list branches: 🌱
./gitlet branch <branch-name>  # create
./gitlet branch  # list


Switch branches: 🔄
./gitlet checkout <branch-name>


Remove a file: ❌
./gitlet rm <file>


Push to remote: 📤
./gitlet push <remote_path>


Pull from remote: 📥
./gitlet pull <remote_path>


Merge a branch into master: 🤝
./gitlet merge <branch-name>


Fork the repository: 🍴
./gitlet fork <fork_path>



🤝 Similarities to Git 🤝



C-Gitlet Command
Git Command



gitlet init
git init


gitlet init --bare
git init --bare


gitlet add
git add


gitlet commit
git commit


gitlet log
git log


gitlet status
git status


gitlet branch
git branch


gitlet checkout
git checkout


gitlet rm
git rm


gitlet push <path>
git push


gitlet pull <path>
git pull


gitlet merge <branch>
git merge <branch>


gitlet fork <path>
(GitHub-style fork)


🚀 New Features 🚀
🌟 Push & Pull: Share your work with remote repos! 📡

push: Sends commits to a remote path, like git push but using local directories. 💾
pull: Grabs remote changes and updates your repo, similar to git pull (no conflict resolution). ⚡

🌟 Merge: Combine feature branches into master! 🤝

merge: Fast-forwards master to another branch’s commit, keeping your trunk up-to-date. 🚂

🌟 Fork: Create a new repository copy! 🍴

fork: Copies your repo to a new path as a bare repository, perfect for collaborating like a GitHub fork. 🌐
Automatically adds the fork as a remote named "fork" for easy push and pull. 🔗

🌟 8-Character Alphanumeric Hashes: 🎨

Objects (blobs, trees, commits) are stored with 8-character alphanumeric hashes (e.g., aB12cD34), matching the format used in our directory listing tool for consistency. 🔒

These make C-Gitlet awesome for collaborative workflows and trunk-based development! 🎉
🌟 Trunk-Based Development 🌟
C-Gitlet supports a trunk-based workflow where master is your main branch (the "trunk"). 🌳

Create feature branches with gitlet branch and work on them. 🌱
Use gitlet merge to bring changes back to master. 🤝
push and pull to sync with remote repos or forks, keeping everyone aligned. 🔄
Perfect for quick, continuous integration without complex merging. 🚀

Unlike Git’s full merge strategies, our merge is a simple fast-forward, ideal for learning and small projects. 🎓
🌟 Fork vs. Branch 🌟
Confused about fork and branch? Let’s clear it up! 😄

Branch (gitlet branch): Creates a new line of development within your repo. Think of it as a new path in the same project. 🌱
Fork (gitlet fork): Creates a new repository by copying your repo to a new path. It’s like starting a separate project you can collaborate with using push and pull. 🍴
Use branch for features in your project, and fork to share or work independently with others. 🤝

📚 Detailed Guide for Beginners 📚
Hey there, newbie! 👋 C-Gitlet is your fun intro to version control! 🌈 Let’s dive in with sparkles! ✨
🛠️ Setting Up Your Repo 🛠️

Compile: 🔨 gcc gitlet.c -o gitlet—builds your tool! 🎸
Initialize: 🚀 ./gitlet init creates .gitlet/, your project’s time machine. ⏳
Use ./gitlet init --bare for a remote-ready repo. 🗄️



📁 Managing Files 📁

Create: 📝 echo "Hello, world!" > hello.txt—make something cool! 🎨
Add: ➕ ./gitlet add hello.txt—stage it like wrapping a gift! 🎁
Commit: ✅ ./gitlet commit "First save"—lock it in with a message! 🧠
Commits get a cool 8-char hash like xY7zW8vU! 🔒



🔍 Checking Stuff 🔍

Status: 🕵️ ./gitlet status—see your branch, staged files, and more. 💉
Log: 📜 ./gitlet log—view your commit history. Time travel! 🕰️

🌿 Branching Fun 🌿

Create Branch: 🌱 ./gitlet branch cool-feature—experiment safely in your repo! ⚠️
List Branches: 📋 ./gitlet branch—see all your paths! 🗺️
Switch: 🔄 ./gitlet checkout cool-feature—jump to it, files update! 🪄

🗑️ Cleaning Up 🗑️

Remove: ❌ ./gitlet rm hello.txt—delete and unstage it. Bye! 👋

🌐 Remote Workflows 🌐

Remote Setup: 🏰 mkdir my-remote; cd my-remote; ../gitlet init --bare—your server castle! 🏰
Push: 📤 ./gitlet push ../my-remote—share commits! ❤️
Pull: 📥 ./gitlet pull ../my-remote—grab updates! 🔄
Careful: Pull overwrites local changes! ⚠️



🤝 Merging & Trunk Workflow 🤝

Merge: 🚂 ./gitlet merge cool-feature—bring your feature branch into master. Keep the trunk rolling! 🌳
Must be on master to merge. It’s a simple fast-forward, no conflicts. 🎉



🍴 Forking for Collaboration 🍴

Fork: 🍴 ./gitlet fork ../my-fork—copy your repo to a new path for independent work or sharing. 🌐
Creates a bare repo and adds it as a remote named "fork" for easy push and pull. 🔗
Unlike branch, it’s a whole new repo—perfect for collaborating with others! 🤝



💡 Newbie Tips 💡

Check status often! 👀
Branches are free—use them for features! 🌟
Forks make new repos for big changes or sharing! 🍴
Remotes and forks are just local paths here. No internet needed! 🏠
Peek in .gitlet/ if curious—objects have 8-char hashes! 🔍
Run gcc test.c -o test && ./test for pass/fail tests. ✅❌

Have fun versioning! 🚀 Mess up? Start fresh with init. 😄 Code away! 👩‍💻👨‍💻
