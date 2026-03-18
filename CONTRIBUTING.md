# Contributing to HexaOS

Thank you for your interest in HexaOS.

HexaOS is developed with a strict separation between:

- `src/` and other HexaOS core directories, which are part of the HexaOS core operating system, and
- `lib/`, which is reserved for third-party libraries and externally licensed components.

## Basic rules

1. Pull requests must target the main development branch used by the project owner.
2. Keep each pull request focused on one feature, fix, or refactor.
3. Only touch files relevant to the proposed change.
4. Third-party libraries must not be added to `src/`. Third-party code belongs in `lib/` and must keep its original license and notices.
5. Code comments inside source files must be written in English.
6. The project owner may reject, rewrite, partially merge, or reimplement any proposed change.

## Important ownership rule

HexaOS core ownership is intentionally centralized.

If you submit a contribution that changes any file **outside** the `lib/` directory, then by submitting that contribution you agree to the following Contributor Assignment.

## Contributor Assignment for HexaOS Core

By submitting any contribution to this repository that modifies any file outside the `lib/` directory, you agree that:

1. **Assignment of rights.** You hereby assign to Martin Macak all right, title, and interest, including all copyrights and copyright applications, in and to your contribution, including any modifications, additions, deletions, updates, fixes, refactors, documentation, tests, and related materials contained in that submission, to the maximum extent permitted by applicable law.

2. **Fallback license if assignment is ineffective.** If any part of the assignment above is held to be invalid, ineffective, or unenforceable under applicable law, you grant Martin Macak a perpetual, irrevocable, worldwide, exclusive, transferable, sublicensable, royalty-free license to use, reproduce, modify, adapt, publish, distribute, relicense, sell, offer for sale, import, make available, and otherwise exploit the contribution without restriction.

3. **Right to relicense.** You expressly authorize Martin Macak to distribute, license, sublicense, dual-license, commercialize, or relicense the contribution under GPL-3.0-only, proprietary terms, commercial terms, or any other license terms, at his sole discretion.

4. **Original work / sufficient rights.** You represent and warrant that:
   - the contribution was created by you, or you otherwise have sufficient rights to submit it under these terms;
   - the contribution does not knowingly violate the rights of any third party;
   - if your employer or any other party may claim rights in the contribution, you have obtained all necessary permissions and waivers before submission.

5. **Moral rights waiver / consent.** To the maximum extent permitted by applicable law, you waive, and agree not to assert, any moral rights, droit moral, or similar rights in the contribution against Martin Macak or HexaOS users. If such rights cannot be waived, you consent to any act or omission that would otherwise infringe them.

6. **No obligation to use.** Martin Macak is not required to accept, merge, publish, or use any contribution.

7. **Public record.** You understand that your submission, authorship record, and associated discussion may be stored publicly and indefinitely as part of the project history.

## Contributions limited to `lib/`

If your pull request only adds or updates content inside `lib/`, then:

- ownership of that third-party code is **not** transferred to Martin Macak by this policy;
- the code must remain under its original upstream license;
- you must preserve all upstream copyright and license notices;
- you must identify the upstream source and license in the pull request.

## Pull request checklist

Before opening a pull request, make sure that:

- [ ] The pull request is based on the current development branch.
- [ ] Only relevant files were changed.
- [ ] No third-party library code was added to `src/`.
- [ ] Any third-party code added to `lib/` keeps its original license and notices.
- [ ] The code builds successfully.
- [ ] I understand and accept the Contributor Assignment terms above for any contribution outside `lib/`.

## Licensing summary

- HexaOS-owned core files are licensed publicly under `GPL-3.0-only` unless stated otherwise.
- Third-party code remains under its own original license.
- The project owner may separately offer proprietary or commercial licenses for HexaOS-owned code.
