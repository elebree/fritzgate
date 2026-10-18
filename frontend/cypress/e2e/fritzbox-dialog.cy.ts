import type { StatusData } from "../../src/interfaces";
import { corsHeaders, statusData } from "../support/api";

const withFritzStatus = (status: string, changes: Partial<StatusData> = {}): StatusData =>
  statusData({ fritz: { host: "fritz.box", user: "demo", status }, ...changes });

const thermostatsSection = () => cy.contains("article", "Thermostats");

describe("FRITZ!Box connection", () => {
  describe("thermostats section", () => {
    it("offers to connect instead of opening the dialog by itself", () => {
      cy.stubApi(withFritzStatus("NOT_CONFIGURED", { thermostats: [] }));
      cy.visit("/");

      // Still closed after the next status poll
      cy.wait("@status");
      cy.wait("@status");
      cy.get("dialog").should("not.exist");

      thermostatsSection().within(() => {
        cy.contains("FritzGate isn't connected to a FRITZ!Box yet.");
        cy.contains("a", "Connect to FRITZ!Box").click();
      });
      cy.contains("dialog[open]", "Connect to FRITZ!Box");
    });

    it("offers to connect before any sensor is found", () => {
      cy.stubApi(withFritzStatus("NOT_CONFIGURED", { thermostats: [], sensors: [] }));
      cy.visit("/");

      thermostatsSection().contains("a", "Connect to FRITZ!Box");
    });

    it("explains a failed login, also in the dialog", () => {
      cy.stubApi(withFritzStatus("FAILURE"));
      cy.visit("/");

      thermostatsSection().within(() => {
        cy.contains("couldn't log in to the FRITZ!Box");
        cy.contains("a", "Connect to FRITZ!Box").click();
      });
      cy.contains("dialog[open]", "failed to connect to FRITZ!Box with the provided credentials");
    });

    it("shows while FritzGate is connecting", () => {
      cy.stubApi(withFritzStatus("CONNECTING", { thermostats: [] }));
      cy.visit("/");

      thermostatsSection().within(() => {
        cy.contains("connecting to FRITZ!Box");
        cy.contains("a", "Connect to FRITZ!Box").should("not.exist");
      });
    });

    it("says when the FRITZ!Box has no thermostats", () => {
      cy.stubApi(withFritzStatus("CONNECTED", { thermostats: [] }));
      cy.visit("/");

      thermostatsSection().within(() => {
        cy.contains("no thermostats found on the FRITZ!Box yet");
        cy.contains("a", "Connect to FRITZ!Box").should("not.exist");
      });
    });
  });

  describe("dialog", () => {
    it("opens from the gear next to the thermostats, filled in", () => {
      cy.stubApi(withFritzStatus("CONNECTED"));
      cy.visit("/");

      thermostatsSection().find('header a[href="#setup"]').click();

      cy.get("dialog[open]").within(() => {
        cy.contains("Update FRITZ!Box credentials");
        cy.get("input[name=ip]").should("have.value", "fritz.box");
        cy.get("input[name=login]").should("have.value", "demo");
        cy.get("input[name=pass]").should("have.value", "");
        cy.get("input[type=submit]").should("be.disabled");
      });
    });
  });

  describe("dialog after Connect", () => {
    let fritzStatus: string;

    /** Serves `initial`, and `afterCredentials` once the credentials are posted, like the firmware */
    const stubConnection = (initial: string, afterCredentials: string) => {
      fritzStatus = initial;
      cy.stubApi(() => withFritzStatus(fritzStatus));
      cy.intercept("POST", "**/api/fritzauth", (req) => {
        fritzStatus = afterCredentials;
        req.reply({ headers: corsHeaders, body: {} });
      }).as("fritzauth");
    };

    // Waits until the app has received `status` from one of its polls every 2 s
    const waitForStatus = (status: string): void => {
      cy.wait("@status").then((interception) => {
        if (interception.response?.body.fritz.status !== status) waitForStatus(status);
      });
    };

    const reportStatus = (status: string) => {
      cy.then(() => (fritzStatus = status));
      waitForStatus(status);
    };

    const connect = () => {
      cy.visit("/#setup");
      cy.get("dialog[open]").within(() => {
        cy.get("input[name=pass]").type("secret");
        cy.get("input[type=submit]").should("be.enabled").click();
      });
      cy.wait("@fritzauth");
    };

    it("sends new credentials and closes once the FRITZ!Box is connected again", () => {
      stubConnection("CONNECTED", "CONFIGURED");
      connect();

      cy.get("@fritzauth")
        .its("request.body")
        // Sent without a JSON content type, so it may arrive as text
        .then((body) => (typeof body === "string" ? JSON.parse(body) : body))
        .should("deep.equal", { host: "fritz.box", user: "demo", pass: "secret" });
      cy.contains("dialog[open]", "connecting to FRITZ!Box");

      waitForStatus("CONFIGURED");
      reportStatus("CONNECTING");
      cy.contains("dialog[open]", "connecting to FRITZ!Box");

      reportStatus("CONNECTED");
      cy.get("dialog").should("not.exist");
    });

    it("closes when FritzGate logs in before its next status poll", () => {
      // On first setup the firmware connects right away, so "connecting" is never seen
      stubConnection("NOT_CONFIGURED", "CONNECTED");
      connect();

      cy.contains("dialog[open]", "connecting to FRITZ!Box");
      cy.get("dialog", { timeout: 10000 }).should("not.exist");
    });

    it("closes when a connection that was already starting is established", () => {
      stubConnection("CONNECTING", "CONFIGURED");
      cy.visit("/#setup");
      cy.contains("dialog[open]", "connecting to FRITZ!Box");

      reportStatus("CONNECTED");
      cy.get("dialog").should("not.exist");
    });

    it("shows the form again when the FRITZ!Box rejects new credentials", () => {
      stubConnection("CONNECTED", "CONFIGURED");
      connect();
      waitForStatus("CONFIGURED");

      reportStatus("FAILURE");
      cy.get("dialog[open]")
        .should("contain", "failed to connect to FRITZ!Box with the provided credentials")
        .find("input[type=submit]")
        .should("exist");
    });

    it("shows the form again when the first login fails right away", () => {
      stubConnection("NOT_CONFIGURED", "FAILURE");
      connect();

      cy.get("dialog[open]", { timeout: 10000 })
        .should("contain", "failed to connect to FRITZ!Box with the provided credentials")
        .find("input[type=submit]")
        .should("exist");
    });
  });
});
