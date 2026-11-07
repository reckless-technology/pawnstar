#!/usr/bin/env bash
# Developer ID signing and notarisation for a macOS release, on a keychain of the job's own, so the runner keeps no
# certificate between jobs. The organisation's apps sign the same way, with the same organisation secrets (vst-plugins'
# tools/sign-mac.sh, which this mirrors; that repository is private, so the steps are written again here).
#
#   tools/sign-macos.sh open                     a keychain made from the secrets below, put first on the user's search
#                                                list (macOS finds a certificate's issuer only in a keychain on it)
#   tools/sign-macos.sh sign [--identifier <id>] [--entitlements <plist>] <path>...
#                                                each Mach-O file or .app signed with the Developer ID Application
#                                                identity, the hardened runtime and a secure timestamp (an app's own
#                                                Mach-O files first, the entitlements on the app alone; a bare binary
#                                                under the identifier given, not its file name), then held to what
#                                                notarisation and Gatekeeper ask
#   tools/sign-macos.sh notarize <zip|dmg|pkg>   sent to Apple's notary service and waited for; anything but Accepted
#                                                fails with Apple's log. A bare binary goes inside a zip; a ticket can
#                                                be stapled only to an .app, a .dmg or a .pkg, which the caller does
#   tools/sign-macos.sh staple <app>             the app's notarisation ticket fetched from Apple and stapled to it, then
#                                                validated
#   tools/sign-macos.sh validate <app>           the ticket stapled to the app validated
#   tools/sign-macos.sh check <path>...          each signed by this team's Developer ID with the hardened runtime and a
#                                                timestamp, and known to Apple as notarised (codesign's online check)
#   tools/sign-macos.sh close                    the keychain deleted and the search list put back (run it always)
#
# Stapling, validating, codesign's online notarisation check and Gatekeeper's assessment all ask Apple's servers, which
# drop connections now and then (stapler's Error 68, NSURLErrorDomain -1005 "The network connection was lost") and
# take a while to serve a ticket just issued (a check seconds after Accepted fails its 'notarized' requirement; the
# assessment calls the code Unnotarized). Each is tried again after 10, 20, 30, 45 and 60 seconds, about three minutes
# in all, but only for those failures: a bad signature, a missing ticket on validation or a rejected submission fails at
# once, and a failure that lasts through every attempt fails the job. RECKLESS_RETRY_DELAYS replaces the waits.
#
# The environment, the organisation secrets: MACOS_DEVELOPER_ID_APPLICATION_P12 (the identity as base64 PKCS #12, in the
# legacy format macOS's security tool reads), MACOS_SIGNING_P12_PASSWORD, MACOS_TEAM_ID, and for notarize
# MACOS_NOTARY_KEY_P8, MACOS_NOTARY_KEY_ID and MACOS_NOTARY_ISSUER_ID (an App Store Connect API key). The keychain's
# password is made at random and never leaves the job. RECKLESS_NOTARY_TIMEOUT bounds the wait (default 3h).
set -euo pipefail

keychain="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/engine-signing.keychain-db"
saved_search_list="${keychain%.keychain-db}.search-list"

# The waits, in seconds, before each further attempt at a call to Apple that failed in a way only time mends.
read -r -a retry_delays <<< "${RECKLESS_RETRY_DELAYS:-10 20 30 45 60}"

# What Apple's network failing looks like in stapler's and codesign's output, and a ticket not yet served.
network_failure='NSURLErrorDomain|network connection was lost|request timed out|timed out|Internet connection appears to be offline|Could not connect to the server|Error 68|Record not found|CloudKit query .* failed'

# run_retrying <what> <transient pattern> <command>...: the command run and its output shown; a failure whose output
# matches the pattern is tried again after each of retry_delays, every attempt logged (a warning on GitHub Actions).
# Any other failure, or one that outlasts the last wait, returns the command's own exit status.
run_retrying() {
    local what=$1 transient=$2 attempt=1 output status delay
    shift 2
    while true; do
        status=0
        output=$("$@" 2>&1) || status=$?
        [ -z "$output" ] || printf '%s\n' "$output"
        if [ "$status" = 0 ]; then
            [ "$attempt" = 1 ] || echo "$what: succeeded on attempt $attempt"
            return 0
        fi
        if ! grep -Eq "$transient" <<< "$output"; then
            echo "$what: failed on attempt $attempt (exit $status), not in a way that waiting mends" >&2
            return "$status"
        fi
        if [ "$attempt" -gt "${#retry_delays[@]}" ]; then
            echo "$what: still failing after $attempt attempts (exit $status)" >&2
            return "$status"
        fi
        delay=${retry_delays[$((attempt - 1))]}
        echo "::warning::$what: attempt $attempt failed (exit $status) on Apple's network or a ticket not yet served;" \
            "trying again in ${delay}s"
        sleep "$delay"
        attempt=$((attempt + 1))
    done
}

# Gatekeeper's verdict on a path, which must be accepted as notarised Developer ID code: an app as it is launched, a
# bare binary by its signature alone (the assessment "execute" is for bundles). A refusal is printed for
# run_retrying, which tries again when it calls the code Unnotarized - a ticket Apple has not served yet.
assess() {
    local path=$1 verdict
    if [[ "$path" == *.app ]]; then
        verdict=$(spctl --assess --type execute -vv "$path" 2>&1) || true
    else
        verdict=$(spctl --assess --type open --context context:primary-signature -vv "$path" 2>&1) || true
    fi
    echo "$verdict"
    grep -q 'accepted' <<< "$verdict" && grep -q 'source=Notarized Developer ID' <<< "$verdict"
}

require() {
    for name in "$@"; do
        [ -n "${!name:-}" ] || { echo "::error::$name is not set: the signing secrets (tools/sign-macos.sh's head)" >&2; exit 1; }
    done
}

application_identity() {
    security find-identity -v -p codesigning "$keychain" 2> /dev/null |
        sed -n 's/.*"\(Developer ID Application: .*\)"/\1/p' | head -n 1
}

# Each Mach-O file under an app's Contents, the main executable excepted - signed before the app, inside out.
nested_code() {
    local app=$1 main
    main="$app/Contents/MacOS/$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$app/Contents/Info.plist")"
    find "$app/Contents" -type f ! -path "$main" -print0 | while IFS= read -r -d '' file; do
        file -b "$file" | grep -q 'Mach-O' && printf '%s\0' "$file"
    done
}

check_one() {
    local path=$1 details
    codesign --verify --strict --deep --verbose=2 "$path"
    details=$(codesign -dv --verbose=2 "$path" 2>&1)
    grep -q "^Authority=Developer ID Application: .*($MACOS_TEAM_ID)$" <<< "$details" ||
        { echo "::error::$path is not signed by team $MACOS_TEAM_ID's Developer ID Application identity" >&2; exit 1; }
    grep -Eq '^CodeDirectory .*flags=0x[0-9a-f]*\(.*runtime.*\)' <<< "$details" ||
        { echo "::error::$path is signed without the hardened runtime" >&2; exit 1; }
    grep -q '^Timestamp=' <<< "$details" || { echo "::error::$path carries no secure timestamp" >&2; exit 1; }
    grep -E '^(Identifier|Authority|Timestamp|CodeDirectory)' <<< "$details" | sed "s|^|  |"
}

case "${1:-}" in
open)
    require MACOS_DEVELOPER_ID_APPLICATION_P12 MACOS_SIGNING_P12_PASSWORD MACOS_TEAM_ID
    password=$(openssl rand -hex 24)
    rm -f "$keychain"
    security create-keychain -p "$password" "$keychain"
    security set-keychain-settings -lut 7200 "$keychain"
    security unlock-keychain -p "$password" "$keychain"
    security list-keychains -d user | tr -d '"' | sed 's/^ *//' > "$saved_search_list"
    # shellcheck disable=SC2046
    security list-keychains -d user -s "$keychain" $(cat "$saved_search_list")
    p12="${keychain%.keychain-db}-application.p12"
    (umask 077; printf '%s' "$MACOS_DEVELOPER_ID_APPLICATION_P12" | base64 --decode > "$p12")
    security import "$p12" -k "$keychain" -P "$MACOS_SIGNING_P12_PASSWORD" -T /usr/bin/codesign > /dev/null
    rm -f "$p12"
    security set-key-partition-list -S apple-tool:,apple: -s -k "$password" "$keychain" > /dev/null
    identity=$(application_identity)
    [ -n "$identity" ] || { echo "::error::no valid Developer ID Application identity in the secrets" >&2; exit 1; }
    case "$identity" in *"($MACOS_TEAM_ID)") ;; *)
        echo "::error::the identity is not team $MACOS_TEAM_ID's: $identity" >&2; exit 1 ;; esac
    echo "signing as: $identity"
    ;;
sign)
    shift
    options=()
    while [ "$#" -gt 0 ]; do
        case "$1" in
        --entitlements) options+=(--entitlements "${2:?--entitlements needs a plist}"); shift 2 ;;
        --identifier) options+=(--identifier "${2:?--identifier needs an identifier}"); shift 2 ;;
        *) break ;;
        esac
    done
    [ "$#" -gt 0 ] || { echo "usage: tools/sign-macos.sh sign [--identifier <id>] [--entitlements <plist>] <path>..." >&2; exit 2; }
    require MACOS_TEAM_ID
    identity=$(application_identity)
    [ -n "$identity" ] || { echo "::error::no signing keychain: run tools/sign-macos.sh open first" >&2; exit 1; }
    sign_one() {
        codesign --force --options runtime --timestamp --keychain "$keychain" --sign "$identity" "$@"
    }
    for path in "$@"; do
        if [[ "$path" == *.app ]]; then
            while IFS= read -r -d '' code; do sign_one "$code"; done < <(nested_code "$path")
        fi
        sign_one ${options[@]+"${options[@]}"} "$path"
        check_one "$path"
        echo "signed $path"
    done
    ;;
notarize)
    file=${2:?usage: tools/sign-macos.sh notarize <zip|dmg|pkg>}
    require MACOS_NOTARY_KEY_P8 MACOS_NOTARY_KEY_ID MACOS_NOTARY_ISSUER_ID
    key="${keychain%.keychain-db}-notary.p8"
    (umask 077; printf '%s\n' "$MACOS_NOTARY_KEY_P8" > "$key")
    notary() { xcrun notarytool "$@" --key "$key" --key-id "$MACOS_NOTARY_KEY_ID" --issuer "$MACOS_NOTARY_ISSUER_ID"; }
    result=$(notary submit "$file" --wait --timeout "${RECKLESS_NOTARY_TIMEOUT:-3h}" --output-format json) || true
    field() { python3 -I -c "import json,sys; print(json.load(sys.stdin).get('$1',''))" <<< "$result" 2> /dev/null || true; }
    id=$(field id)
    status=$(field status)
    echo "notarisation ${id:-(no id)}: ${status:-$(field message)}"
    if [ "$status" != "Accepted" ]; then
        [ -n "$id" ] && notary log "$id" || true
        rm -f "$key"
        echo "::error::Apple's notary service did not accept $file (status '${status:-none}'); its log is above" >&2
        exit 1
    fi
    rm -f "$key"
    ;;
staple)
    app=${2:?usage: tools/sign-macos.sh staple <app>}
    run_retrying "stapling $app" "$network_failure" xcrun stapler staple "$app" ||
        { echo "::error::the notarisation ticket could not be stapled to $app" >&2; exit 1; }
    run_retrying "validating $app's ticket" "$network_failure" xcrun stapler validate "$app" ||
        { echo "::error::the ticket stapled to $app does not validate" >&2; exit 1; }
    ;;
validate)
    app=${2:?usage: tools/sign-macos.sh validate <app>}
    run_retrying "validating $app's ticket" "$network_failure" xcrun stapler validate "$app" ||
        { echo "::error::the ticket stapled to $app does not validate" >&2; exit 1; }
    ;;
check)
    shift
    require MACOS_TEAM_ID
    for path in "$@"; do
        # The signature first, which is local and never tried again: a bad one fails here at once.
        check_one "$path"
        # Asks Apple whether this code's notarisation ticket exists: the check Gatekeeper makes of a download that
        # carries no stapled ticket (a bare binary can carry none). With the signature already verified, failing its
        # 'notarized' requirement means Apple has no ticket for it - not yet, seconds after Accepted - or could not
        # be asked; either is tried again.
        run_retrying "the online notarisation check of $path" \
            "test-requirement: code failed to satisfy specified code requirement|$network_failure" \
            codesign --verify --check-notarization -R='notarized' --verbose=2 "$path" ||
            { echo "::error::Apple does not know $path as notarised" >&2; exit 1; }
        # Gatekeeper's own verdict, which asks Apple too for code with no stapled ticket.
        run_retrying "Gatekeeper's assessment of $path" "source=Unnotarized Developer ID|$network_failure" assess "$path" ||
            { echo "::error::Gatekeeper does not accept $path as notarised Developer ID code" >&2; exit 1; }
    done
    ;;
close)
    if [ -f "$saved_search_list" ]; then
        # shellcheck disable=SC2046
        security list-keychains -d user -s $(cat "$saved_search_list")
        rm -f "$saved_search_list"
    fi
    security delete-keychain "$keychain" 2> /dev/null || true
    rm -f "${keychain%.keychain-db}"-*.p12 "${keychain%.keychain-db}-notary.p8"
    echo "signing keychain closed"
    ;;
*)
    echo "usage: tools/sign-macos.sh open | sign [--identifier <id>] [--entitlements <plist>] <path>... | notarize <file> |" \
        "staple <app> | validate <app> | check <path>... | close" >&2
    exit 2
    ;;
esac
