/**
 * script.js
 * Entry point — initialises all modules, handles tab switching,
 * clock form, hostname form, event log, display preview.
 * 
 * Authors: @liiseng
 * Assisted by @claude
 * Created: 8-05-2026
 *
 * Disclaimer: This code is part of the E-Ink Mini Clock project
 * and is provided under the GPLv3 License. See LICENSE file for details.
 */

// ── Helpers ──────────────────────────────────────────────
function el(id) {
  return document.getElementById(id);
}

function setText(id, v) {
  var e = el(id);
  if (e) e.textContent = v;
}

function setVal(id, v) {
  var e = el(id);
  if (e) e.value = v;
}

function setCheck(id, v) {
  var e = el(id);
  if (e) e.checked = !!v;
}

function showAlert(msg, type) {
  var a = document.getElementById("globalAlert");
  if (!a) {
    a = document.createElement("div");
    a.id = "globalAlert";
    a.style.cssText =
      "position:fixed;top:14px;left:50%;transform:translateX(-50%);z-index:999;min-width:220px;text-align:center";
    document.body.appendChild(a);
  }
  a.className = "alert alert-" + (type || "ok");
  a.textContent = msg;
  a.style.display = "flex";
  clearTimeout(a._t);
  a._t = setTimeout(function () {
    a.style.display = "none";
  }, 3000);
}


// ── Tab switching ─────────────────────────────────────────
var tabs = document.querySelectorAll(".tab-btn");
var panels = document.querySelectorAll(".tab-panel");
tabs.forEach(function (btn) {
  btn.addEventListener("click", function () {
    tabs.forEach(function (b) {
      b.classList.remove("active");
    });
    panels.forEach(function (p) {
      p.classList.remove("active");
    });
    btn.classList.add("active");
    el("panel-" + btn.dataset.tab).classList.add("active");
  });
});

// ── Eye toggle ───────────────────────────────────────────
var eye = el("eyeBtn"),
  pw = el("pass");
eye.addEventListener("click", function () {
  pw.type = pw.type === "password" ? "text" : "password";
  eye.textContent = pw.type === "password" ? "●" : "○";
});

// ── OTA file picker label ────────────────────────────────
el("firmware").addEventListener("change", function () {
  setVal(
    "otaFilename",
    this.files[0] ? this.files[0].name : "No file selected",
  );
});

// ── RSSI → bar count ─────────────────────────────────────
function rssiBars(rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  return 1;
}
function renderBars(containerId, count) {
  var c = el(containerId);
  if (!c) return;
  var spans = c.querySelectorAll("span");
  spans.forEach(function (s, i) {
    s.classList.toggle("on", i < count);
  });
}

// ── GET /config — pre-populate all form fields ───────────
fetch("/config")
  .then(function (r) {
    return r.json();
  })
  .then(function (d) {
    setVal('utcOffset', d.utcOffset);
    setVal('refreshMin', d.refreshMin);
    setVal('ntpSyncDays', d.ntpSyncDays);
    setVal('ntpReSyncDays', d.ntpReSyncDays);
    setVal('quietStart', d.quietStart);
    setVal('quietEnd', d.quietEnd);
    setCheck('hour12', d.hour12);
    setCheck('quietEnabled', d.quietEnabled);
    setCheck('powerSave', d.powerSave);
    setCheck('showBattPct', d.showBattPct);
    setCheck('showHum', d.showHum);
    setCheck('showTemp', d.showTemp);
    setCheck('showRSSI', d.showRSSI);
    setVal('ssid', d.ssid);
    setText('hostNameSide', d.hostname);
    setText('currentHostname', d.hostname);
  })
  .catch(function () { });

// ── GET /status — poll every 5 s ─────────────────────────
function pollStatus() {
  fetch("/status")
    .then(function (r) {
      return r.json();
    })
    .then(function (d) {

      // Device info panel
      setText('fwVerVal', d.firmware);
      setText('otaCurVer', d.firmware);
      setText('macVal', d.mac);
      setText('ipAddrVal', d.ip);
      setText('ipAddrValSide', d.ip);
      setText('hostNameSide', d.hostname);
      setText('heapVal', d.heap + ' KB');
      setText('flashVal', d.flashMB + ' MB');
      setText('fsVal', d.fs);
      setText('uptimeVal2', d.uptime);
      setText('uptimeVal', d.uptime);
      setText('rssiVal', d.rssi + ' dBm');
      setText('rssiVal2', d.rssi + ' dBm');
      setText('lastRefresh', d.lastRefresh);
      setText('cpuVal', d.cpu + ' MHz');
      setText('chipVal', d.chip);
      setText('sdkVal', d.sdk);

      // Mobile strip values
      setText('batValM', d.batPct !== undefined ? d.batPct + '%' : '--%');
      setText('batIconM', d.charging ? '⚡' : d.batPct > 60 ? '🔋' : d.batPct > 20 ? '🪫' : '🔴');
      setText('tempValM', d.temp !== undefined ? d.temp + '°C' : '--°C');
      setText('humValM', d.hum !== undefined ? d.hum + '%' : '--%');
      setText('rssiValM', d.rssi !== undefined ? d.rssi + ' dBm' : '-- dBm');

      // IP color
      var ipEl = el('ipAddrVal');
      if (ipEl) ipEl.className = 'info-val ' + (d.ip !== '0.0.0.0' ? 'ok' : 'er');

      // NTP needs class update too:
      var ntpEl = el('ntpVal');
      if (ntpEl) {
        ntpEl.textContent = d.ntpSynced ? 'Synced' : 'Not synced';
        ntpEl.className = 'info-val ' + (d.ntpSynced ? 'ok' : 'er');
      }

      // Header stats
      setText("liveTime", d.time);
      setText("liveTimeMobile", d.time);

      // Temp & humidity
      setText('tempVal', d.temp !== undefined ? d.temp + '°C' : '--°C');
      setText('humVal', d.hum !== undefined ? d.hum + '%' : '--%');

      // Battery
      if (d.batPct !== undefined) {
        setText('batVal', d.batPct + '%');
        var batEl = el('batVal');
        if (batEl) batEl.className = 'hstat-val ' + (d.batPct > 20 ? 'ok' : 'er');
        setText('batIcon', d.charging ? '⚡' : (d.batPct > 60 ? '🔋' : d.batPct > 20 ? '🪫' : '🔴'));
      }

      // RSSI
      var bars = rssiBars(d.rssi);
      var rssiTxt = d.rssi + " dBm";
      var rssiEl = el("rssiVal");
      if (rssiEl) rssiEl.childNodes[0].textContent = rssiTxt + " ";
      renderBars("headerBars", bars);

      // Battery graph
      recordBattery(d.batPct);

      // Device panel live values
      setText("heapVal", d.heap + " KB");
      setText("ipVal", d.ip);
      setText("ntpVal", d.ntpSynced ? "Synced" : "Not synced");
      var ntpEl = el("ntpVal");
      if (ntpEl) {
        ntpEl.className = "info-val " + (d.ntpSynced ? "ok" : "er");
      }
    })
    .catch(function () { });
}
pollStatus();
setInterval(pollStatus, 5000);

// ── GET /scan — populate network list ────────────────────
function doScan() {
  var nets = el("nets");
  nets.innerHTML =
    "<div style=\"color:var(--muted);font-size:12px;padding:10px;font-family:'DM Mono',monospace\">Scanning…</div>";
  fetch("/scan")
    .then(function (r) {
      return r.json();
    })
    .then(function (list) {
      nets.innerHTML = "";
      if (!list.length) {
        nets.innerHTML =
          '<div style="color:var(--muted);font-size:12px;padding:10px">No networks found.</div>';
        return;
      }
      list.sort(function (a, b) {
        return b.rssi - a.rssi;
      });
      list.forEach(function (n) {
        var bars = rssiBars(n.rssi);
        var div = document.createElement("div");
        div.className = "net";
        div.dataset.ssid = n.ssid;
        var barSpans = ["b1", "b2", "b3", "b4"]
          .map(function (cls, i) {
            return (
              '<span class="' +
              cls +
              (i < bars ? " on" : "") +
              '"></span>'
            );
          })
          .join("");
        div.innerHTML =
          '<span class="net-name">' +
          n.ssid +
          "</span>" +
          '<span class="net-meta">' +
          (n.open ? "" : '<span class="lock">🔒</span>') +
          '<span class="bars">' +
          barSpans +
          "</span>" +
          "</span>";
        div.addEventListener("click", function () {
          document.querySelectorAll(".net").forEach(function (r) {
            r.classList.remove("sel");
          });
          div.classList.add("sel");
          setVal("ssid", n.ssid);
          el("pass").focus();
        });
        nets.appendChild(div);
      });
    })
    .catch(function () {
      nets.innerHTML =
        '<div style="color:var(--er-tx);font-size:12px;padding:10px">Scan failed.</div>';
    });
}
doScan();

// Rescan button (wire up if present)
var rescanBtn = el("rescanBtn");
if (rescanBtn) rescanBtn.addEventListener("click", doScan);

// ── POST /save-clock ─────────────────────────────────────
var clockForm = el("clockForm");
if (clockForm) {
  clockForm.addEventListener("submit", function (e) {
    e.preventDefault();
    var fd = new FormData(clockForm);
    fetch("/save-clock", { method: "POST", body: new URLSearchParams(fd) })
      .then(function (r) {
        return r.json();
      })
      .then(function (d) {
        showAlert(
          d.ok ? "Clock settings saved! Rebooting" : "saved failed, try again!" + d.msg,
          d.ok ? "ok" : "er",
        );
      })
      .catch(function () {
        showAlert("Save failed", "er");
      });
  });
}

// ── POST /save-wifi ──────────────────────────────────────
var wifiForm = el("wifiForm");
if (wifiForm) {
  wifiForm.addEventListener("submit", function (e) {
    e.preventDefault();
    var fd = new FormData(wifiForm);
    showAlert("Saving… device will reboot", "warn");
    fetch("/save-wifi", { method: "POST", body: fd })
      .then(function (r) {
        return r.json();
      })
      .then(function (d) {
        showAlert(
          d.ok ? "✓ " + d.msg : "✗ " + d.msg,
          d.ok ? "ok" : "er",
        );
      })
      .catch(function () {
        showAlert("Save failed", "er");
      });
  });
}

// ── POST /save-hostname ───────────────────────────────────
var hostnameBtn = el("hostnameBtn");
if (hostnameBtn) {
  hostnameBtn.addEventListener("click", function () {
    var fd = new FormData();
    fd.append("hostnameVal", el("hostnameVal").value);
    fetch("/save-hostname", { method: "POST", body: fd })
      .then(function (r) {
        return r.json();
      })
      .then(function (d) {
        showAlert(
          d.ok ? "Hostname saved!" : "Failed",
          d.ok ? "ok" : "er",
        );
      })
      .catch(function () {
        showAlert("Save failed", "er");
      });
  });
}

var hnInput = document.getElementById('hostnameVal');
var hnCount = document.getElementById('hostnameCount');
if (hnInput && hnCount) {
  function updateHnCount() {
    hnCount.textContent = hnInput.value.length;
    hnCount.style.color = hnInput.value.length >= 12 ? 'var(--er-tx)' : 'var(--muted)';
  }
  hnInput.addEventListener('input', updateHnCount);
  updateHnCount();  // init on load
}

// ── POST /action ──────────────────────────────────────────
function doAction(action, msg) {
  if (!confirm(msg)) return;
  fetch("/action?cmd=" + action, { method: "POST" })
    .then(function (r) {
      return r.json();
    })
    .then(function (d) {
      var type = d.ok
        ? action === "restart" || action === "factory"
          ? "warn"
          : "ok"
        : "er";
      var a = document.getElementById("globalAlert");
      if (!a) {
        a = document.createElement("div");
        a.id = "globalAlert";
        a.style.cssText =
          "position:fixed;top:14px;left:50%;transform:translateX(-50%);z-index:999;min-width:220px;text-align:center";
        document.body.appendChild(a);
      }
      a.className = "alert alert-" + type;
      a.textContent = d.msg || (d.ok ? "✓ Done" : "✗ Failed");
      a.style.display = "flex";
      clearTimeout(a._t);
      a._t = setTimeout(function () {
        a.style.display = "none";
      }, 3500);
    })
    .catch(function () {
      console.warn("Action failed:", action);
    });
}

// ── OTA drag-drop setup ───────────────────────────
var _otaFile = null;
var dropZone = document.getElementById('otaDropZone');

if (dropZone) {
  dropZone.addEventListener('dragover', function (e) {
    e.preventDefault();
    dropZone.classList.add('drag-over');
  });
  dropZone.addEventListener('dragleave', function () {
    dropZone.classList.remove('drag-over');
  });
  dropZone.addEventListener('drop', function (e) {
    e.preventDefault();
    dropZone.classList.remove('drag-over');
    var file = e.dataTransfer.files[0];
    if (file) setOtaFile(file);
  });
  document.getElementById('firmware').addEventListener('change', function () {
    if (this.files[0]) setOtaFile(this.files[0]);
  });
}

function setOtaFile(file) {
  _otaFile = file;
  var label = document.getElementById('otaFilename');
  label.textContent = file.name + ' (' + (file.size / 1024).toFixed(1) + ' KB)';
  label.style.display = 'block';
}

// ── Flash from local file ─────────────────────────
function startOTA() {
  if (!_otaFile) { showAlert('Select a .bin file first', 'er'); return; }
  if (!confirm('Flash ' + _otaFile.name + '?\nDevice will restart after upload.')) return;

  var prog = document.getElementById('otaProgress');
  var fill = document.getElementById('otaFill');
  var pct = document.getElementById('otaPct');
  var label = document.getElementById('otaProgressLabel');

  prog.classList.remove('ota-hidden');
  fill.style.width = '0%';
  fill.style.background = '';
  pct.textContent = '0%';
  if (label) label.textContent = 'Uploading…';

  var xhr = new XMLHttpRequest();

  xhr.upload.addEventListener('progress', function (e) {
    if (e.lengthComputable) {
      var p = Math.round(e.loaded / e.total * 100);
      fill.style.width = p + '%';
      pct.textContent = p + '%';
    }
  });

  xhr.onload = function () {
    if (xhr.status === 200) {
      fill.style.width = '100%';
      pct.textContent = '100%';
      if (label) label.textContent = 'Done — restarting…';
      showAlert('✓ Upload complete — device restarting', 'ok');
      pollForReboot();
    } else {
      if (label) label.textContent = 'Server error: ' + xhr.status;
      fill.style.background = 'var(--er-tx)';
      showAlert('✗ Upload failed (' + xhr.status + ')', 'er');
    }
  };

  xhr.onerror = function () {
    if (label) label.textContent = 'Upload error';
    fill.style.background = 'var(--er-tx)';
    showAlert('✗ Upload failed', 'er');
  };

  xhr.ontimeout = function () {
    if (label) label.textContent = 'Timeout';
    showAlert('✗ Upload timed out', 'er');
  };

  xhr.timeout = 60000;

  var fd = new FormData();
  fd.append('image', _otaFile);
  xhr.open('POST', '/ota');
  xhr.send(fd);
}

// ── Flash from URL ────────────────────────────────
function startUrlOTA() {
  var url = document.getElementById('otaUrl').value.trim();
  if (!url.startsWith('http')) { showAlert('Enter a valid http(s):// URL', 'er'); return; }
  if (!confirm('Flash firmware from:\n' + url + '\n\nDevice will restart.')) return;
  showAlert('⏳ Sending URL to device…', 'warn');

  fetch('/ota-url', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'url=' + encodeURIComponent(url)
  })
    .then(function (r) { return r.json(); })
    .then(function (d) {
      if (d.ok) {
        showAlert('⏳ Flashing… polling for reboot', 'warn');
        pollForReboot(null);
      } else {
        showAlert('✗ ' + (d.msg || 'Failed'), 'er');
      }
    })
    .catch(function () { showAlert('✗ Could not reach device', 'er'); });
}

// ── Check for update ──────────────────────────────
function checkOtaUpdate() {
  var statusEl = document.getElementById('otaCheckStatus');
  var latestEl = document.getElementById('otaLatestVer');
  var alertEl = document.getElementById('otaUpdateAlert');
  var serverBlk = document.getElementById('otaServerBlock');

  statusEl.textContent = 'Checking…';
  latestEl.textContent = '…';
  alertEl.className = 'alert ota-hidden';
  serverBlk.style.display = 'none';

  function tryFetch(attempts) {
    if (attempts <= 0) {
      statusEl.textContent = 'Timeout';
      showAlert('✗ Check timed out', 'er');
      return;
    }
    fetch('/check-update')
      .then(function (r) { return r.json(); })
      .then(function (d) {
        if (d.pending) {
          // Still fetching on ESP side, poll again in 2s
          setTimeout(function () { tryFetch(attempts - 1); }, 2000);
          return;
        }
        if (!d.version) {
          statusEl.textContent = 'Failed';
          showAlert('✗ ' + (d.msg || 'Could not reach server'), 'er');
          return;
        }
        // Got a real result
        latestEl.textContent = d.version;
        var cur = (document.getElementById('otaCurVer').textContent || '').trim();
        var latest = d.version.replace('v', '').trim();
        console.log('cur:', cur, 'latest:', latest);
        var upToDate = cur === latest;

        statusEl.textContent = upToDate ? 'Up to date' : 'Update available!';
        alertEl.className = 'alert ' + (upToDate ? 'alert-ok' : 'alert-warn');
        alertEl.textContent = upToDate
          ? 'You are running the latest firmware.'
          : 'A new version is available — click below to install.';

        if (!upToDate && d.url) {
          window._otaServerUrl = d.url;
          document.getElementById('otaUrl').value = d.url;
          serverBlk.style.display = 'block';
        }
      })
      .catch(function () {
        statusEl.textContent = 'Error';
        showAlert('✗ Check failed', 'er');
      });
  }

  tryFetch(10);  // up to 10 polls × 2s = 20s max wait
}

// ── Auto-install from server ──────────────────────
function startServerOTA() {
  if (!window._otaServerUrl) {
    showAlert('Run "Check for Update" first', 'er'); return;
  }
  if (!confirm('Download and install from server?\nDevice will restart after flashing.')) return;

  var prog = document.getElementById('otaServerProgress');
  var fill = document.getElementById('otaServerFill');
  var label = document.getElementById('otaServerLabel');
  var pct = document.getElementById('otaServerPct');
  prog.classList.remove('ota-hidden');
  label.textContent = 'Sending to device…';

  var fd = new FormData();
  fd.append('url', window._otaServerUrl);
  fetch('/ota-url', { method: 'POST', body: fd })
    .then(function (r) { return r.json(); })
    .then(function (d) {
      if (d.ok) {
        fill.style.width = '100%';
        pct.textContent = '✓';
        label.textContent = 'Flashing — polling for reboot…';
        pollForReboot(label);
      } else {
        label.textContent = 'Failed: ' + (d.msg || 'unknown');
        fill.style.background = 'var(--er-tx)';
        showAlert('✗ ' + (d.msg || 'OTA failed'), 'er');
      }
    })
    .catch(function () {
      label.textContent = 'Request failed';
      showAlert('✗ Could not reach device', 'er');
    });
}

// ── Poll until device comes back after reboot ─────
function pollForReboot(labelEl) {
  var attempts = 0;
  var timer = setInterval(function () {
    attempts++;
    if (attempts > 40) {           // ~2 min timeout
      clearInterval(timer);
      showAlert('⚠ Device not responding — check manually', 'warn');
      return;
    }
    fetch('/status').then(function (r) {
      if (r.ok) {
        clearInterval(timer);
        if (labelEl) labelEl.textContent = 'Done — device is back online!';
        showAlert('✓ Update complete! Device is back online.', 'ok');
      }
    }).catch(function () {
      if (labelEl) labelEl.textContent = 'Rebooting… (' + attempts + ')';
    });
  }, 3000);
}
