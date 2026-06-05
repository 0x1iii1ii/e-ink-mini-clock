(function () {
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
    // type: 'ok' | 'er' | 'warn'
    var a = el("globalAlert");
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
        setText("fwVal", d.firmware);
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
  doScan(); // scan on load

  // Rescan button (wire up if present)
  var rescanBtn = el("rescanBtn");
  if (rescanBtn) rescanBtn.addEventListener("click", doScan);

  // ── POST /save-clock ─────────────────────────────────────
  var clockForm = el("clockForm");
  if (clockForm) {
    clockForm.addEventListener("submit", function (e) {
      e.preventDefault();
      var fd = new FormData(clockForm);
      // Manually add unchecked checkboxes as absent (default HTML behaviour)
      fetch("/save-clock", { method: "POST", body: new URLSearchParams(fd) })
        .then(function (r) {
          return r.json();
        })
        .then(function (d) {
          showAlert(
            d.ok ? "✓ Clock settings saved!" : "✗ " + d.msg,
            d.ok ? "ok" : "er",
          );
        })
        .catch(function () {
          showAlert("✗ Save failed", "er");
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
          showAlert("✗ Save failed", "er");
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
            d.ok ? "✓ Hostname saved!" : "✗ Failed",
            d.ok ? "ok" : "er",
          );
        })
        .catch(function () {
          showAlert("✗ Save failed", "er");
        });
    });
  }
})();

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

// ── OTA upload ────────────────────────────────────────────
function startOTA() {
  var file = document.getElementById("firmware").files[0];
  if (!file) {
    alert("Please select a .bin file first.");
    return;
  }
  if (
    !confirm(
      "Flash " + file.name + "?\nDevice will restart after upload.",
    )
  )
    return;

  var xhr = new XMLHttpRequest();
  var prog = document.getElementById("otaProgress");
  var fill = document.getElementById("otaFill");
  var pct = document.getElementById("otaPct");
  prog.classList.remove("ota-hidden");

  xhr.upload.addEventListener("progress", function (e) {
    if (e.lengthComputable) {
      var p = Math.round((e.loaded / e.total) * 100);
      fill.style.width = p + "%";
      pct.textContent = p + "%";
    }
  });
  xhr.onload = function () {
    pct.textContent = "100% — Restarting…";
    fill.style.width = "100%";
  };
  xhr.onerror = function () {
    pct.textContent = "Upload failed";
    fill.style.background = "var(--er-tx)";
  };

  var fd = new FormData();
  fd.append("image", file); // HTTPUpdateServer expects field name "image"
  xhr.open("POST", "/ota");
  xhr.send(fd);
}

// ── URL OTA ───────────────────────────────────────────
function startUrlOTA() {
  var url = el('otaUrl').value.trim();
  if (!url.startsWith('http')) { showAlert('Enter a valid https:// URL', 'er'); return; }
  if (!confirm('Flash firmware from:\n' + url + '\n\nDevice will restart.')) return;
  var fd = new FormData(); fd.append('url', url);
  showAlert('⏳ Starting URL OTA…', 'warn');
  fetch('/ota-url', { method: 'POST', body: fd })
    .then(function (r) { return r.json(); })
    .then(function (d) { showAlert(d.ok ? '✓ ' + d.msg : '✗ ' + d.msg, d.ok ? 'ok' : 'er'); })
    .catch(function () { showAlert('✗ OTA request failed', 'er'); });
}

// ── Check update ──────────────────────────────────────
function checkOtaUpdate() {
  setText('otaCheckStatus', 'Checking…');
  setText('otaLatestVer', '…');
  fetch('/check-update')
    .then(function (r) { return r.json(); })
    .then(function (d) {
      if (d.version) {
        setText('otaLatestVer', d.version);
        var cur = el('otaCurVer').textContent.replace('v', '');
        var same = d.version.replace('v', '') === cur;
        setText('otaCheckStatus', same ? 'Up to date ✓' : 'Update available!');
        var alertEl = el('otaUpdateAlert');
        alertEl.classList.remove('ota-hidden');
        alertEl.className = 'alert ' + (same ? 'alert-ok' : 'alert-warn');
        alertEl.textContent = same
          ? '✓ You are running the latest firmware.'
          : '⚠ New version ' + d.version + ' available — enter URL below to update.';
        if (!same && d.url) {
          // Show the one-click update block and store the URL
          el('otaServerBlock').style.display = 'block';
          window._otaServerUrl = d.url;  // server must return "url" field in version.json
          el('otaUrl').value = d.url;
        }
        el('otaServerBlock').style.display = 'none';
      } else {
        setText('otaCheckStatus', 'Failed');
        showAlert('✗ ' + (d.msg || 'Could not check for updates'), 'er');
      }
    })
    .catch(function () { setText('otaCheckStatus', 'Error'); showAlert('✗ Check failed', 'er'); });
}

function startServerOTA() {
  if (!window._otaServerUrl) {
    showAlert('No update URL — run Check for Update first', 'er');
    return;
  }
  if (!confirm('Download and install firmware from server?\nDevice will restart after flashing.')) return;

  var prog = el('otaServerProgress');
  var fill = el('otaServerFill');
  var label = el('otaServerLabel');
  var pct = el('otaServerPct');
  prog.classList.remove('ota-hidden');
  label.textContent = 'Sending URL to device…';
  pct.textContent = '';

  var fd = new FormData();
  fd.append('url', window._otaServerUrl);

  fetch('/ota-url', { method: 'POST', body: fd })
    .then(function (r) { return r.json(); })
    .then(function (d) {
      if (d.ok) {
        label.textContent = 'Flashing… device will restart';
        fill.style.width = '100%';
        pct.textContent = '✓';
        showAlert('✓ OTA started — reconnect in ~30 s', 'ok');
        // Poll /status every 3 s — when it responds again the device is back
        var pollBack = setInterval(function () {
          fetch('/status').then(function (r) {
            if (r.ok) {
              clearInterval(pollBack);
              label.textContent = 'Done — device is back online!';
              showAlert('✓ Update complete!', 'ok');
            }
          }).catch(function () {
            label.textContent = 'Rebooting…';
          });
        }, 3000);
      } else {
        label.textContent = 'Failed: ' + (d.msg || 'unknown error');
        fill.style.background = 'var(--er-tx)';
        showAlert('✗ ' + (d.msg || 'OTA failed'), 'er');
      }
    })
    .catch(function () {
      label.textContent = 'Request failed';
      fill.style.background = 'var(--er-tx)';
      showAlert('✗ Could not reach device', 'er');
    });
}

// ── Battery graph ─────────────────────────────────────
var _batHistory = [];   // { t: timestamp, v: pct }
var _batGraphTimer = null;

function recordBattery(pct) {
  if (isNaN(pct)) return;
  _batHistory.push({ t: Date.now(), v: pct });
  // Keep last 60 points (1 hour at 1-min poll)
  if (_batHistory.length > 60) _batHistory.shift();
  drawBatGraph();
}

function drawBatGraph() {
  var canvas = el('batCanvas');
  if (!canvas || !_batHistory.length) return;

  var empty = el('batGraphEmpty');
  if (empty) empty.style.display = _batHistory.length < 2 ? 'flex' : 'none';
  if (_batHistory.length < 2) return;

  // Set canvas resolution to match CSS size
  canvas.width = canvas.offsetWidth * window.devicePixelRatio;
  canvas.height = canvas.offsetHeight * window.devicePixelRatio;

  var ctx = canvas.getContext('2d');
  var W = canvas.width, H = canvas.height;
  var dpr = window.devicePixelRatio;
  ctx.clearRect(0, 0, W, H);

  var vals = _batHistory.map(function (p) { return p.v; });
  var minV = Math.max(0, Math.min.apply(null, vals) - 5);
  var maxV = Math.min(100, Math.max.apply(null, vals) + 5);
  var range = maxV - minV || 1;

  function xOf(i) { return (i / (_batHistory.length - 1)) * W; }
  function yOf(v) { return H - ((v - minV) / range) * H * 0.85 - H * 0.05; }

  // Fill under line
  var grad = ctx.createLinearGradient(0, 0, 0, H);
  grad.addColorStop(0, 'rgba(245,158,11,.35)');
  grad.addColorStop(1, 'rgba(245,158,11,.02)');
  ctx.beginPath();
  ctx.moveTo(xOf(0), H);
  _batHistory.forEach(function (p, i) { ctx.lineTo(xOf(i), yOf(p.v)); });
  ctx.lineTo(xOf(_batHistory.length - 1), H);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  // Line
  ctx.beginPath();
  _batHistory.forEach(function (p, i) {
    i === 0 ? ctx.moveTo(xOf(i), yOf(p.v)) : ctx.lineTo(xOf(i), yOf(p.v));
  });
  ctx.strokeStyle = 'rgba(245,158,11,.9)';
  ctx.lineWidth = 1.5 * dpr;
  ctx.lineJoin = 'round';
  ctx.stroke();

  // Latest dot
  var last = _batHistory[_batHistory.length - 1];
  ctx.beginPath();
  ctx.arc(xOf(_batHistory.length - 1), yOf(last.v), 3 * dpr, 0, Math.PI * 2);
  ctx.fillStyle = '#f59e0b';
  ctx.fill();

  setText('batGraphMin', 'min: ' + Math.min.apply(null, vals) + '%');
  setText('batGraphMax', 'max: ' + Math.max.apply(null, vals) + '%');
  setText('batGraphPoints', _batHistory.length + ' pts');
}

// Poll battery every 60 s while portal is open
function startBatGraph() {
  if (_batGraphTimer) return;
  _batGraphTimer = setInterval(function () {
    fetch('/status')
      .then(function (r) { return r.json(); })
      .then(function (d) { if (d.batPct !== undefined) recordBattery(d.batPct); })
      .catch(function () { });
  }, 60000);
}

// Seed with first available value from regular status poll
// Hook into existing pollStatus — add this line inside its .then():
// recordBattery(d.batPct);
// (add that one line to pollStatus where batPct is consumed)

startBatGraph();
window.addEventListener('resize', drawBatGraph);