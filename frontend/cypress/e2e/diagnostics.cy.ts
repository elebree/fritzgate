import type { Diagnostics } from "../../src/interfaces";
import { corsHeaders, diagnosticsAfter, diagnosticsData, statusData } from "../support/api";

// Answers GET /api/diagnostics like a board in the state that state() returns when asked
const stubDiagnostics = (state: () => Diagnostics) =>
  cy
    .intercept({ method: "GET", pathname: "/api/diagnostics" }, (req) => {
      req.reply({ headers: corsHeaders, body: diagnosticsAfter(state(), Number(req.query.after ?? 0)) });
    })
    .as("diagnostics");

describe("diagnostics", () => {
  beforeEach(() => {
    cy.stubApi(statusData());
  });

  it("opens from the navigation bar and explains the state of the board", () => {
    cy.visit("/");

    cy.get('nav a[href="#diagnostics"]').click();

    cy.wait("@diagnostics");
    cy.location("hash").should("eq", "#diagnostics");
    cy.get("dialog[open] table").within(() => {
      cy.contains("tr", "Firmware").should("contain", "v1.2.0");
      cy.contains("tr", "Uptime").should("contain", "1 d 2 h");
      cy.contains("tr", "Last start").should("contain", "watchdog: no thermostat data from the FRITZ!Box for 5 minutes");
      cy.contains("tr", "Free memory")
        .should("contain", "90 KB, lowest 71 KB")
        .and("contain", `PSRAM ${(2600).toLocaleString()} KB`);
      cy.contains("tr", "Wi-Fi signal").should("contain", "-61 dBm, good");
      cy.contains("tr", "FRITZ!OS").should("contain", "8.26");
      cy.contains("tr", "FRITZ!Box API").should("contain", "Smart Home REST API");
    });
  });

  it("explains a brownout and a board that isn't logged in", () => {
    stubDiagnostics(() => diagnosticsData({ resetReason: "brownout", rssi: -80, fritzOsVersion: "", fritzApi: null }));
    cy.visit("/#diagnostics");

    cy.contains("dialog tr", "Last start").should("contain", "supply voltage too low");
    cy.contains("dialog tr", "Wi-Fi signal").should("contain", "-80 dBm, weak");
    cy.contains("dialog tr", "FRITZ!OS").should("contain", "unknown");
    cy.contains("dialog tr", "FRITZ!Box API").should("contain", "not logged in");
  });

  it("shows the log with the newest line last, and marks warnings and errors", () => {
    cy.visit("/#diagnostics");

    cy.get("dialog .log .line").should("have.length", 5);
    cy.get("dialog .log .line").last().should("contain", "GET /api/status");
    cy.contains("dialog .log .line", "Skip offset").should("have.class", "level-W");
    cy.contains("dialog .log .line", "overview/units: 500").should("have.class", "level-E");
  });

  it("adds the lines that FritzGate logs while the dialog is open, asking only for those", () => {
    const board = diagnosticsData();
    stubDiagnostics(() => board);
    cy.visit("/#diagnostics");
    cy.get("dialog .log .line").should("have.length", 5);

    cy.then(() => {
      board.log.lines.push("[ 95000][I] Clock moved by 2 s");
      board.log.next++;
      board.uptime += 2;
    });

    cy.get("dialog .log .line").should("have.length", 6).last().should("contain", "Clock moved by 2 s");
    cy.get("@diagnostics.all").its("0.request.query.after").should("eq", "0");
    cy.get("@diagnostics.all").its("1.request.query.after").should("eq", "5");
    cy.get("dialog footer").should("not.exist");
  });

  it("follows the new lines, unless the log is scrolled up to read older ones", () => {
    const board = diagnosticsData();
    board.log.lines = Array.from({ length: 60 }, (_, i) => `[${String(1000 * i).padStart(6)}][I] line ${i}`);
    board.log.next = 60;
    const addLine = (text: string) => {
      board.log.lines.push(`[ 99000][I] ${text}`);
      board.log.next++;
      board.uptime += 2;
    };
    const atBottom = ($log: JQuery<HTMLElement>) => {
      const log = $log[0];
      expect(log.scrollTop + log.clientHeight).to.be.closeTo(log.scrollHeight, 2);
    };
    stubDiagnostics(() => board);
    cy.visit("/#diagnostics");
    cy.get("dialog .log .line").should("have.length", 60);
    cy.get("dialog .log").should(atBottom);

    cy.get("dialog .log").scrollTo("top");
    cy.then(() => addLine("while reading"));
    cy.get("dialog .log .line").should("have.length", 61);
    cy.get("dialog .log").invoke("scrollTop").should("eq", 0);

    cy.get("dialog .log").scrollTo("bottom");
    cy.then(() => addLine("while following"));
    cy.get("dialog .log .line").should("have.length", 62);
    cy.get("dialog .log").should(atBottom);
  });

  it("keeps the lines from before a restart of FritzGate", () => {
    let board = diagnosticsData();
    stubDiagnostics(() => board);
    cy.visit("/#diagnostics");
    cy.get("dialog .log .line").should("have.length", 5);

    // the board starts again with a few lines, numbered from 0
    cy.then(() => {
      board = diagnosticsData({
        uptime: 4,
        resetReason: "crash",
        log: {
          first: 0,
          next: 2,
          lines: ["[   410][S] FritzGate v1.2.0 starting", "[   900][I] WiFi connecting: demo"],
        },
      });
    });

    cy.contains("dialog .log .line", "FritzGate restarted").should("have.class", "marker");
    cy.get("dialog .log .line").should("have.length", 5 + 1 + 2).last().should("contain", "WiFi connecting");
    cy.contains("dialog tr", "Last start").should("contain", "crash of the firmware");
  });

  it("marks the lines that FritzGate didn't keep until the dialog asked", () => {
    const board = diagnosticsData();
    stubDiagnostics(() => board);
    cy.visit("/#diagnostics");
    cy.get("dialog .log .line").should("have.length", 5);

    // 100 lines later, the board keeps only the last 3
    cy.then(() => {
      board.log = { first: 102, next: 105, lines: ["[ 99000][I] a", "[ 99100][I] b", "[ 99200][I] c"] };
      board.uptime += 60;
    });

    cy.contains("dialog .log .line", "97 lines not kept by FritzGate").should("have.class", "marker");
    cy.get("dialog .log .line").should("have.length", 5 + 1 + 3);
  });

  it("is wider than the other dialogs on big screens, for long log lines", () => {
    cy.viewport(1600, 900);
    cy.visit("/#diagnostics");
    cy.get("dialog[open] article").invoke("outerWidth").should("be.greaterThan", 1000);

    cy.visit("/#setup");
    cy.get("dialog[open] article").invoke("outerWidth").should("be.at.most", 700);
  });

  it("closes with the browser's back button", () => {
    cy.visit("/");
    cy.get('nav a[href="#diagnostics"]').click();
    cy.get("dialog[open]");

    cy.go("back");

    cy.get("dialog").should("not.exist");
    cy.location("hash").should("eq", "");
  });

  it("explains a failed load, and tries again", () => {
    let available = false;
    cy.intercept({ method: "GET", pathname: "/api/diagnostics" }, (req) => {
      if (available) {
        req.reply({ headers: corsHeaders, body: diagnosticsData() });
      } else {
        req.reply({ statusCode: 500, headers: corsHeaders });
      }
    });
    cy.visit("/#diagnostics");

    cy.get("dialog").should("contain", "couldn't load the diagnostics");

    cy.then(() => (available = true));

    cy.contains("dialog tr", "Firmware").should("contain", "v1.2.0");
    cy.get("dialog").should("not.contain", "couldn't load the diagnostics");
  });
});
