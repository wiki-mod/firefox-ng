/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class UnselectedTabHoverChild extends JSWindowActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "Browser:UnselectedTabHover":
        Services.obs.notifyObservers(
          this.contentWindow,
          "unselected-tab-hover",
          message.data.hovered
        );
        break;
      case "Browser:UnselectedTabIdleHint":
        // Patch 7b: warmup hint from chrome on tab hover. Pref-gating
        // happens chrome-side so by the time we see the message the
        // user has opted in. Best-effort; ChromeUtils returns silently
        // on unsupported platforms.
        ChromeUtils.markProcessIdleHint(message.data.active);
        break;
    }
  }

  handleEvent(event) {
    this.sendAsyncMessage("UnselectedTabHover:Toggle", {
      enable: event.type == "UnselectedTabHover:Enable",
    });
  }
}
