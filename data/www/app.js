var ws;
var motorEnabled = false;
var stations = [];
var defaultStationCount = 0;
var currentStation = -1;

function setText(id, val) {
  var el = document.getElementById(id);
  if (el) el.textContent = val;
}

function connect() {
  ws = new WebSocket("ws://" + location.host + "/ws");

  ws.onopen = function() {
    document.getElementById("status").textContent = "Connected";
    document.getElementById("status").className = "status online";
  };

  ws.onclose = function() {
    document.getElementById("status").textContent = "Disconnected";
    document.getElementById("status").className = "status offline";
    setTimeout(connect, 2000);
  };

  ws.onmessage = function(e) {
    var d = JSON.parse(e.data);

    if (d.type === "sensors") {
      setText("battery", d.battery.toFixed(2));
      setText("temp", d.temp.toFixed(1));
      setText("ldr", d.ldr.toFixed(0));
      setText("time", d.time);
      if (d.ntpSynced) {
        var ago = Math.floor(d.ntpSyncAgo / 60);
        setText("ntpStatus", "Synced " + ago + "m ago");
      } else {
        setText("ntpStatus", "Not synced");
      }
      setText("accelX", d.accelX.toFixed(3));
      setText("accelY", d.accelY.toFixed(3));
      setText("accelZ", d.accelZ.toFixed(3));
      if (d.radioPlaying) {
        setText("radioName", d.radioName || "--");
      } else {
        setText("radioName", "Stopped");
      }

      var chg = document.getElementById("charging");
      if (chg) {
        chg.textContent = d.charging ? "Charging" : "Not charging";
        chg.className = d.charging ? "badge charging" : "badge not-charging";
      }

      var volSlider = document.getElementById("radioVol");
      if (volSlider && !volSlider.matches(":active")) {
        var pct = Math.round((d.radioVolume || 0) * 100);
        volSlider.value = pct;
        setText("volval", pct);
      }

      if (d.radioStation !== currentStation) {
        currentStation = d.radioStation;
        renderStationList();
      }
    }
    else if (d.type === "stations") {
      stations = d.stations;
      defaultStationCount = d.defaultCount || 0;
      renderStationList();
    }
  };
}

function renderStationList() {
  var list = document.getElementById("stationList");
  if (!list) return;
  list.innerHTML = "";
  for (var i = 0; i < stations.length; i++) {
    var row = document.createElement("div");
    var active = (i === currentStation);
    row.className = "station-row" + (active ? " active" : "");
    var isBuiltin = i < defaultStationCount;
    var badge = isBuiltin ? '<span class="badge-builtin">Built-in</span>' : '';
    var delBtn = isBuiltin ? '' :
      '<button onclick="removeStation(' + i + ')" class="btn-sm stop">Del</button>';
    row.innerHTML =
      '<button onclick="playStation(' + i + ')" class="btn-sm' + (active ? ' playing' : ' play') + '">' +
        (active ? '&#9654;' : '&#9654;') + '</button>' +
      '<span class="station-name">' + stations[i].name + '</span>' +
      badge +
      '<span class="station-url">' + stations[i].url + '</span>' +
      delBtn;
    list.appendChild(row);
  }
}

function send(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(obj));
  }
}

function sendCmd(cmd) {
  send({ cmd: cmd });
}

function sendMotor(id, value) {
  value = parseInt(value);
  document.getElementById("m" + id + "val").textContent = value;
  send({ cmd: "motor", id: id, value: value });
}

function toggleMotor() {
  motorEnabled = !motorEnabled;
  var btn = document.getElementById("motorToggle");
  btn.textContent = motorEnabled ? "ON" : "OFF";
  btn.className = motorEnabled ? "toggle on" : "toggle";
  send({ cmd: "motor_enable", value: motorEnabled });
}

function sendLed() {
  var r = parseInt(document.getElementById("ledR").value);
  var g = parseInt(document.getElementById("ledG").value);
  var b = parseInt(document.getElementById("ledB").value);
  document.getElementById("rval").textContent = r;
  document.getElementById("gval").textContent = g;
  document.getElementById("bval").textContent = b;
  document.getElementById("ledPreview").style.background =
    "rgb(" + r + "," + g + "," + b + ")";
  send({ cmd: "led", r: r, g: g, b: b });
}

function sendBrightness(value) {
  value = parseInt(value);
  document.getElementById("brval").textContent = value;
  send({ cmd: "led_brightness", value: value });
}

function playStation(index) {
  send({ cmd: "radio_play", station: index });
}

function sendVolume(value) {
  value = parseInt(value);
  setText("volval", value);
  send({ cmd: "radio_volume", value: value / 100.0 });
}

function addStation() {
  var name = document.getElementById("newStName").value.trim();
  var url = document.getElementById("newStUrl").value.trim();
  if (name && url) {
    send({ cmd: "station_add", name: name, url: url });
    document.getElementById("newStName").value = "";
    document.getElementById("newStUrl").value = "";
  }
}

function setTime() {
  var input = document.getElementById("setTimeInput");
  if (!input || !input.value) return;
  var d = new Date(input.value);
  send({
    cmd: "set_time",
    year: d.getFullYear(), month: d.getMonth() + 1, day: d.getDate(),
    hour: d.getHours(), minute: d.getMinutes(), second: d.getSeconds()
  });
}

function setNtp() {
  var server = document.getElementById("ntpServer").value.trim();
  if (server) {
    send({ cmd: "set_ntp", server: server });
  }
}

function setTz(tz) {
  if (tz) {
    send({ cmd: "set_tz", tz: tz });
  }
}

function removeStation(index) {
  send({ cmd: "station_remove", index: index });
}

connect();
