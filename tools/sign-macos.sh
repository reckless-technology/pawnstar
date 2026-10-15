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
#   tools/sign-macos.sh check <path>...          each signed by this team's Developer ID with the hardened runtime and a
#                                                timestamp, and known to Apple as notarised (codesign's online check)
#   tools/sign-macos.sh close                    the keychain deleted and the search list put back (run it always)
#
# The environment, the organisation secrets: MACOS_DEVELOPER_ID_APPLICATION_P12 (the identity as base64 PKCS #12, in the
# legacy format macOS's security tool reads), MACOS_SIGNING_P12_PASSWORD, MACOS_TEAM_ID, and for notarize
# MACOS_NOTARY_KEY_P8, MACOS_NOTARY_KEY_ID and MACOS_NOTARY_ISSUER_ID (an App Store Connect API key). The keychain's
# password is made at random and never leaves the job. RECKLESS_NOTARY_TIMEOUT bounds the wait (default 3h).
set -euo pipefail

keychain="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/engine-signing.keychain-db"
saved_search_list="${keychain%.keychain-db}.search-list"

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
check)
    shift
    require MACOS_TEAM_ID
    for path in "$@"; do
        check_one "$path"
        # Asks Apple whether this code's notarisation ticket exists: the check Gatekeeper makes of a download that
        # carries no stapled ticket (a bare binary can carry none).
        codesign --verify --check-notarization -R='notarized' --verbose=2 "$path"
        # Gatekeeper's own verdict: an app as it is launched, a bare binary by its signature alone (the assessment
        # "execute" is for bundles). Both must say accepted, source=Notarized Developer ID.
        if [[ "$path" == *.app ]]; then
            verdict=$(spctl --assess --type execute -vv "$path" 2>&1) || true
        else
            verdict=$(spctl --assess --type open --context context:primary-signature -vv "$path" 2>&1) || true
        fi
        echo "$verdict"
        if ! grep -q 'accepted' <<< "$verdict" || ! grep -q 'source=Notarized Developer ID' <<< "$verdict"; then
            echo "::error::Gatekeeper does not accept $path as notarised Developer ID code" >&2
            exit 1
        fi
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
        "check <path>... | close" >&2
    exit 2
    ;;
esac
