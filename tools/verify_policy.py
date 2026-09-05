from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> None:
    document = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    assert document["version"] == 1
    assert document["requirements"]["multiplier"] == 0.1
    assert document["requirements"]["preserveFirstFanLevel"] is True
    assert document["upgradeCosts"]["preserveCurrencyType"] is True
    assert document["upgradeCosts"]["singleDigitCurrencies"] == ["CP", "Candy"]
    assert document["upgradeCosts"]["preserveCurveDigits"] is True
    assert document["cosmetics"]["purchasablePrice"] == 10
    assert document["cooldowns"]["skillResetSeconds"] == 300
    assert document["starPass"]["seasonCount"] == 13
    assert document["timers"]["storage"] == "absolute-unix-seconds"


if __name__ == "__main__":
    main()
