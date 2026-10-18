import type { StatusData } from "../../src/interfaces";
import { corsHeaders, statusData } from "../support/api";

const withFritzStatus = (status: string): StatusData =>
  statusData({ fritz: { host: "fritz.box", user: "demo", status } });

describe("FRITZ!Box dialog", () => {
  it("opens from the gear next to the thermostats, filled in", () => {
    cy.stubApi(withFritzStatus("CONNECTED"));
    cy.visit("/");

    cy.contains("article", "Thermostats").find('a[href="#setup"]').click();

    cy.get("dialog[open]").within(() => {
      cy.contains("Update FRITZ!Box credentials");
      cy.get("input[name=ip]").should("have.value", "fritz.box");
      cy.get("input[name=login]").should("have.value", "demo");
      cy.get("input[name=pass]").should("have.value", "");
      cy.get("input[type=submit]").should("be.disabled");
    });
  });

  it("opens once by itself while the FRITZ!Box isn't configured", () => {
    cy.clock(Date.now(), ["setTimeout", "clearTimeout"]);
    cy.stubApi(statusData({ fritz: { host: "", user: "", status: "NOT_CONFIGURED" }, thermostats: [] }));
    cy.visit("/");

    cy.contains("dialog[open]", "Connect to FRITZ!Box");
    cy.get('dialog button[aria-label="Close"]').click();
    cy.get("dialog").should("not.exist");

    // The next status poll doesn't open it again
    cy.tick(2000);
    cy.wait("@status");
    cy.wait("@status");
    cy.get("dialog").should("not.exist");
  });

  it("shows when the FRITZ!Box rejected the credentials", () => {
    cy.stubApi(withFritzStatus("FAILURE"));
    cy.visit("/");

    cy.contains("dialog[open]", "failed to connect to FRITZ!Box with the provided credentials");
  });

  describe("with new credentials", () => {
    let fritzStatus: string;

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

    beforeEach(() => {
      fritzStatus = "CONNECTED";
      cy.stubApi(() => withFritzStatus(fritzStatus));
      // Like the firmware, accepting new credentials drops the old connection
      cy.intercept("POST", "**/api/fritzauth", (req) => {
        fritzStatus = "CONFIGURED";
        req.reply({ headers: corsHeaders, body: {} });
      }).as("fritzauth");
      cy.visit("/#setup");
      cy.wait("@status");

      cy.get("dialog[open]").within(() => {
        cy.get("input[name=pass]").type("secret");
        cy.get("input[type=submit]").should("be.enabled").click();
      });
    });

    it("sends them and closes once the FRITZ!Box is connected again", () => {
      cy.wait("@fritzauth")
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

    it("shows the form again when the FRITZ!Box rejects them", () => {
      cy.wait("@fritzauth");
      waitForStatus("CONFIGURED");

      reportStatus("FAILURE");
      cy.get("dialog[open]")
        .should("contain", "failed to connect to FRITZ!Box with the provided credentials")
        .find("input[type=submit]")
        .should("exist");
    });
  });
});
