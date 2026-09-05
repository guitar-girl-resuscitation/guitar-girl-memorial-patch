import importlib.util
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "transform_manifest", ROOT / "tools" / "transform_manifest.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)
A = MODULE.A


class ManifestTransformTests(unittest.TestCase):
    def test_launcher_is_rebased_while_retired_initializers_are_disabled(self):
        source_xml = f'''<manifest xmlns:android="{MODULE.ANDROID}" package="com.neowiz.game.guitargirl">
          <uses-permission android:name="android.permission.POST_NOTIFICATIONS"/>
          <uses-permission android:name="android.permission.INTERNET"/>
          <application android:label="old">
            <activity android:name="com.google.firebase.MessagingUnityPlayerActivity" android:exported="true">
              <intent-filter><category android:name="android.intent.category.LAUNCHER"/><action android:name="android.intent.action.MAIN"/></intent-filter>
            </activity>
            <provider android:name="com.google.firebase.provider.FirebaseInitProvider" android:authorities="com.neowiz.game.guitargirl.firebase"/>
            <meta-data android:name="com.google.android.gms.ads.flag.OPTIMIZE_INITIALIZATION" android:value="true"/>
            <activity android:name="com.pmangplus.ui.activity.PPPurchaseGoogleLocalPrice_port"/>
            <activity android:name="com.pmangplus.ui.activity.PPPurchaseGoogleLocalPrice"/>
          </application>
        </manifest>'''
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "AndroidManifest.xml"
            output = Path(directory) / "out.xml"
            source.write_text(source_xml, encoding="utf-8")
            MODULE.transform(
                source, output, "com.neowiz.game.guitargirl",
                "org.guitargirlresuscitation.memorial", "Memorial", True,
                800001, "8.0.0-memorial.1",
            )
            root = ET.parse(output).getroot()
            permissions = {
                node.get(A + "name") for node in root.findall("uses-permission")
            }
            self.assertNotIn("android.permission.POST_NOTIFICATIONS", permissions)
            self.assertIn("android.permission.INTERNET", permissions)
            application = root.find("application")
            self.assertEqual("true", application.get(A + "usesCleartextTraffic"))
            activity = application.find("activity")
            provider = application.find("provider")
            metadata = application.find("meta-data")
            self.assertEqual("com.google.firebase.MessagingUnityPlayerActivity", activity.get(A + "name"))
            self.assertEqual("true", activity.get(A + "enabled"))
            self.assertEqual("false", activity.get(A + "exported"))
            self.assertFalse(MODULE.is_launcher(activity))
            self.assertEqual("false", provider.get(A + "enabled"))
            self.assertIsNone(metadata.get(A + "enabled"))
            self.assertEqual(
                "org.guitargirlresuscitation.memorial.firebase",
                provider.get(A + "authorities"),
            )
            bootstrap = [
                node for node in application.findall("provider")
                if node.get(A + "name") == "org.guitargirlresuscitation.memorial.MemorialBootstrapProvider"
            ]
            self.assertEqual(1, len(bootstrap))
            startup = [
                node for node in application.findall("activity")
                if node.get(A + "name") == "org.guitargirlresuscitation.memorial.MemorialStartupActivity"
            ]
            self.assertEqual(1, len(startup))
            self.assertTrue(MODULE.is_launcher(startup[0]))
            nodes = list(application)
            target = next(node for node in nodes if node.get(A + "name") == MODULE.LOCAL_PRICE_ACTIVITY)
            self.assertEqual("@android:style/Theme.NoDisplay", target.get(A + "theme"))
            aliases = application.findall("activity-alias")
            self.assertEqual(2, len(aliases))
            for alias in aliases:
                self.assertIn(alias.get(A + "name"), MODULE.RETIRED_PRICE_ACTIVITIES)
                self.assertEqual(MODULE.LOCAL_PRICE_ACTIVITY, alias.get(A + "targetActivity"))
                self.assertEqual("false", alias.get(A + "exported"))
                self.assertLess(nodes.index(target), nodes.index(alias))
            self.assertFalse(any(node.get(A + "name") in MODULE.RETIRED_PRICE_ACTIVITIES
                                 for node in application.findall("activity")))

    def test_missing_or_duplicate_price_activity_fails_closed(self):
        for names in [[], [MODULE.RETIRED_PRICE_ACTIVITIES[0]],
                      list(MODULE.RETIRED_PRICE_ACTIVITIES) + [MODULE.RETIRED_PRICE_ACTIVITIES[0]]]:
            app = ET.Element("application")
            for name in names:
                ET.SubElement(app, "activity", {A + "name": name})
            with self.assertRaises(ValueError):
                MODULE.replace_price_activities(app)
