"""Deterministically transform an apktool-decoded AndroidManifest.xml.

This script contains no game data. The patcher invokes it only after source and
split hashes have passed the compatibility manifest.
"""

from __future__ import annotations

import argparse
import xml.etree.ElementTree as ET
from pathlib import Path


ANDROID = "http://schemas.android.com/apk/res/android"
A = f"{{{ANDROID}}}"
ET.register_namespace("android", ANDROID)

RETIRED_COMPONENT_PREFIXES = (
    "com.google.firebase.",
    "com.google.android.gms.",
    "com.android.billingclient.",
    "com.facebook.",
    "com.appsflyer.",
    "com.pmangplus.ui.activity.firebase.",
)

RETIRED_PERMISSIONS = {
    "com.android.vending.BILLING",
    # The memorial build has no remote push channel. Leaving this permission
    # makes Android 13+ interrupt the first local launch with a notification
    # prompt even though all Firebase messaging components are disabled.
    "android.permission.POST_NOTIFICATIONS",
    "com.google.android.gms.permission.AD_ID",
    "android.permission.ACCESS_ADSERVICES_AD_ID",
    "android.permission.ACCESS_ADSERVICES_ATTRIBUTION",
    "android.permission.ACCESS_ADSERVICES_TOPICS",
    "android.permission.ACCESS_ADSERVICES_CUSTOM_AUDIENCE",
}

# This is the stock Unity host Activity, not a remote Firebase service.  The
# Firebase C++ plugin keeps a Java reference supplied by this subclass and
# aborts on recent Android releases when the game is hosted by a bare
# UnityPlayerActivity instead.  Keep the lifecycle bridge but remove its
# launcher filter; MemorialStartupActivity remains the only public launcher.
UNITY_HOST_ACTIVITY = "com.google.firebase.MessagingUnityPlayerActivity"
LOCAL_PRICE_ACTIVITY = "org.guitargirlresuscitation.memorial.LocalPriceActivity"
RETIRED_PRICE_ACTIVITIES = (
    "com.pmangplus.ui.activity.PPPurchaseGoogleLocalPrice_port",
    "com.pmangplus.ui.activity.PPPurchaseGoogleLocalPrice",
)


def replace_price_activities(application: ET.Element) -> None:
    """Keep explicit SDK Intent names while routing both to our no-display callback.

    Android resolves an activity-alias before instantiating its target: the
    original Billing Activity's onCreate is never executed. Target must precede
    both aliases. Input APK hashes are checked by the caller before this step.
    """
    originals = [node for node in application.findall("activity")
                 if node.get(A + "name") in RETIRED_PRICE_ACTIVITIES]
    if sorted(node.get(A + "name") for node in originals) != sorted(RETIRED_PRICE_ACTIVITIES):
        raise ValueError("expected both stock price-query activities exactly once")
    if any(node.get(A + "name") == LOCAL_PRICE_ACTIVITY for node in application):
        raise ValueError("offline price activity already present")
    for node in originals:
        application.remove(node)
    ET.SubElement(application, "activity", {
        A + "name": LOCAL_PRICE_ACTIVITY,
        A + "enabled": "true", A + "exported": "false",
        A + "theme": "@android:style/Theme.NoDisplay",
    })
    for name in RETIRED_PRICE_ACTIVITIES:
        ET.SubElement(application, "activity-alias", {
            A + "name": name, A + "targetActivity": LOCAL_PRICE_ACTIVITY,
            A + "enabled": "true", A + "exported": "false",
        })


def is_launcher(component: ET.Element) -> bool:
    for intent_filter in component.findall("intent-filter"):
        actions = {node.get(A + "name") for node in intent_filter.findall("action")}
        categories = {node.get(A + "name") for node in intent_filter.findall("category")}
        if (
            "android.intent.action.MAIN" in actions
            and "android.intent.category.LAUNCHER" in categories
        ):
            return True
    return False


def transform(
    source: Path,
    output: Path,
    old_package: str,
    new_package: str,
    label: str,
    base: bool,
    version_code: int,
    version_name: str,
) -> None:
    tree = ET.parse(source)
    root = tree.getroot()
    if root.tag != "manifest" or root.get("package") != old_package:
        raise ValueError("manifest package precondition failed")

    root.set("package", new_package)
    root.set(A + "versionCode", str(version_code))
    root.set(A + "versionName", version_name)
    for element in root.iter():
        for key, value in list(element.attrib.items()):
            if old_package in value:
                element.set(key, value.replace(old_package, new_package))

    for permission in list(root.findall("uses-permission")):
        if permission.get(A + "name") in RETIRED_PERMISSIONS:
            root.remove(permission)

    application = root.find("application")
    if application is None:
        if base:
            raise ValueError("base manifest has no application")
        output.parent.mkdir(parents=True, exist_ok=True)
        tree.write(output, encoding="utf-8", xml_declaration=True)
        return
    if not base:
        output.parent.mkdir(parents=True, exist_ok=True)
        tree.write(output, encoding="utf-8", xml_declaration=True)
        return
    application.set(A + "label", label)
    # The embedded Thrift server is plain HTTP on a randomized 127.0.0.1
    # endpoint. Android 9+ blocks even loopback cleartext unless the app opts
    # in. The native loader separately denies every non-loopback connect(2).
    application.set(A + "usesCleartextTraffic", "true")
    launcher_rebased = False
    for component in application:
        if component.tag not in {"activity", "activity-alias", "service", "receiver", "provider"}:
            continue
        name = component.get(A + "name", "")
        if name.startswith(RETIRED_COMPONENT_PREFIXES):
            if (
                component.tag == "activity"
                and name == UNITY_HOST_ACTIVITY
                and is_launcher(component)
            ):
                component.set(A + "enabled", "true")
                component.set(A + "exported", "false")
                for intent_filter in list(component.findall("intent-filter")):
                    actions = {node.get(A + "name") for node in intent_filter.findall("action")}
                    categories = {node.get(A + "name") for node in intent_filter.findall("category")}
                    if "android.intent.action.MAIN" in actions and \
                            "android.intent.category.LAUNCHER" in categories:
                        component.remove(intent_filter)
                launcher_rebased = True
            else:
                component.set(A + "enabled", "false")
                component.set(A + "exported", "false")

    if not launcher_rebased:
        raise ValueError("supported launcher activity was not found")
    replace_price_activities(application)
    startup = ET.SubElement(
        application,
        "activity",
        {
            A + "name": "org.guitargirlresuscitation.memorial.MemorialStartupActivity",
            A + "enabled": "true",
            A + "exported": "true",
            A + "launchMode": "singleTask",
            A + "screenOrientation": "portrait",
            A + "theme": "@android:style/Theme.Material.NoActionBar",
        },
    )
    startup_filter = ET.SubElement(startup, "intent-filter")
    ET.SubElement(startup_filter, "action", {A + "name": "android.intent.action.MAIN"})
    ET.SubElement(
        startup_filter,
        "category",
        {A + "name": "android.intent.category.LAUNCHER"},
    )

    provider_name = "org.guitargirlresuscitation.memorial.MemorialBootstrapProvider"
    if any(
        child.tag == "provider" and child.get(A + "name") == provider_name
        for child in application
    ):
        raise ValueError("bootstrap provider is already present")
    ET.SubElement(
        application,
        "provider",
        {
            A + "name": provider_name,
            A + "authorities": new_package + ".ggfm.bootstrap",
            A + "directBootAware": "false",
            A + "exported": "false",
            A + "initOrder": "10000",
        },
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    tree.write(output, encoding="utf-8", xml_declaration=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--old-package", required=True)
    parser.add_argument("--new-package", required=True)
    parser.add_argument("--label", required=True)
    parser.add_argument("--base", action="store_true")
    parser.add_argument("--version-code", type=int, required=True)
    parser.add_argument("--version-name", required=True)
    args = parser.parse_args()
    transform(
        args.source,
        args.output,
        args.old_package,
        args.new_package,
        args.label,
        args.base,
        args.version_code,
        args.version_name,
    )


if __name__ == "__main__":
    main()
