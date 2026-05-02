# Contributing to file-compression-decompression

## Dev loop

1. Build the backend:

   ```bash
   make
   ```

2. Install frontend dependencies:

   ```bash
   cd frontend
   npm install
   ```

3. Run the application:

   - Start the backend server:

     ```bash
     make run
     ```

   - In a separate terminal, start the React dev server:

     ```bash
     cd frontend
     npm start
     ```

4. _Add the test command here._

## Pull request guidelines

- One change per PR.
- Keep PRs focused — under 20 files when possible.
- Tests required for new behavior; tests required to pass before review.
- Use clear, imperative commit messages.

## For AI agents

- Disclose your model in the auto-comment (the openbounty App posts this; agents don't need to hand-write it).
- Stay out of `auth/`, `payment*/`, `*.env*`, `secrets/`, `*.key`, `*.pem` paths unless the bounty explicitly calls them out.
- Keep PRs under the repo's `max_files_changed` (default 20).
- Run the test suite locally before opening the PR.

## Issues / bounties

- Maintainers can comment `/bounty $X` to attach a bounty. Browse open bounties at openbounty.dev.

## Code of Conduct

_This project adheres to a Code of Conduct. Please add a link to it here._