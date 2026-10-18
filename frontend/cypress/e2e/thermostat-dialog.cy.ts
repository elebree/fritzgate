import { celsius, corsHeaders, statusData, unusedSensor } from "../support/api";

describe("thermostat dialog", () => {
  beforeEach(() => {
    const data = statusData();
    data.sensors.push(unusedSensor);
    cy.stubApi(data);
  });

  it("opens after holding a thermostat gauge, with a filling ring meanwhile", () => {
    cy.clock(Date.now(), ["setTimeout", "clearTimeout"]);
    cy.visit("/");

    cy.pressGauge("Kitchen");
    cy.tick(500);
    cy.gauge("Kitchen").find(".hold-progress.active").should("exist");
    cy.get("dialog").should("not.exist");

    cy.tick(100);
    cy.get("dialog[open]").should("contain", "Kitchen");
    cy.location("hash").should("eq", "#thermostat-16");
  });

  it("ignores a short tap", () => {
    cy.clock(Date.now(), ["setTimeout", "clearTimeout"]);
    cy.visit("/");

    cy.pressGauge("Kitchen");
    cy.tick(300);
    cy.releaseGauge("Kitchen");
    cy.tick(1000);

    cy.get("dialog").should("not.exist");
    cy.gauge("Kitchen").find(".hold-progress.active").should("not.exist");
  });

  it("opens from the hidden button for keyboard and screen reader users", () => {
    cy.visit("/");

    cy.contains(".hold-button", "Configure Kitchen").click({ force: true });

    cy.get("dialog[open]").should("contain", "Kitchen");
  });

  it("blocks the context menu of a press-and-hold, but not of a right click", () => {
    cy.visit("/");
    // Whether the page cancelled a context menu opened on the element
    const contextMenu = (selector: string) =>
      cy.get(selector).first().then(($element) => {
        const event = new $element[0].ownerDocument.defaultView!.MouseEvent("contextmenu", {
          bubbles: true,
          cancelable: true,
        });
        $element[0].dispatchEvent(event);
        return event.defaultPrevented;
      });

    contextMenu(".holdable").should("eq", false);

    // A touch press-and-hold counts as a right click, which arrives once the dialog is open
    cy.pressGauge("Kitchen");
    cy.get("dialog[open]");
    contextMenu("dialog").should("eq", true);
  });

  it("shows the thermostat and its current reference sensor", () => {
    cy.visit("/#thermostat-16");

    cy.get("dialog[open]").within(() => {
      cy.get("header").should("contain", "Kitchen").and("contain", "FRITZ!DECT 301 77777 0707001");
      cy.get("header").should("contain", `offset +${celsius(1)}`);
      cy.get("select").should("have.value", "GVH5075_DDDD");
      cy.contains("button", "Save").should("be.disabled");
    });
  });

  it("marks the sensors other thermostats already use", () => {
    cy.visit("/#thermostat-20");

    cy.get("dialog select").within(() => {
      cy.get('option[value="GVH5075_AAAA"]').should("contain", "used by Bedroom");
      cy.get('option[value="GVH5075_EEEE"]').should("not.contain", "used by");
      cy.get('option[value=""]').should("contain", "Built-in thermostat sensor");
    });

    cy.get("dialog select").select("GVH5075_AAAA");
    cy.get("dialog").should("contain", "Also the reference sensor for Bedroom.");
  });

  it("saves the reference sensor only when Save is pressed", () => {
    cy.visit("/#thermostat-20");

    cy.get("dialog select").select("GVH5075_EEEE");
    cy.get("@binding.all").should("have.length", 0);
    cy.contains("dialog button", "Save").should("be.enabled").click();

    cy.wait("@binding")
      .its("request.body")
      .should("deep.equal", { id: "20", referenceSensor: "GVH5075_EEEE" });
    cy.get("dialog").should("not.exist");
  });

  it("keeps the choice when the status refreshes", () => {
    cy.visit("/#thermostat-20");
    cy.get("dialog select").select("GVH5075_EEEE");

    cy.wait("@status");
    cy.wait("@status");

    cy.get("dialog select").should("have.value", "GVH5075_EEEE");
  });

  it("discards the choice on Cancel", () => {
    cy.visit("/#thermostat-20");

    cy.get("dialog select").select("GVH5075_EEEE");
    cy.contains("dialog button", "Cancel").click();

    cy.get("dialog").should("not.exist");
    cy.get("@binding.all").should("have.length", 0);
  });

  it("stays open with an error when saving fails", () => {
    cy.intercept("POST", "**/api/thermostat", { statusCode: 500, headers: corsHeaders }).as(
      "binding"
    );
    cy.visit("/#thermostat-20");

    cy.get("dialog select").select("GVH5075_EEEE");
    cy.contains("dialog button", "Save").click();

    cy.wait("@binding");
    cy.get("dialog[open]")
      .should("contain", "couldn't save the reference sensor")
      .find("select")
      .should("have.value", "GVH5075_EEEE");
  });

  it("closes with the close button", () => {
    cy.visit("/#thermostat-16");

    cy.get('dialog button[aria-label="Close"]').click();

    cy.get("dialog").should("not.exist");
  });

  it("closes with a click outside the card, but not inside it", () => {
    cy.visit("/#thermostat-16");

    cy.get("dialog header").click();
    cy.get("dialog[open]");

    cy.get("dialog").click("topLeft");
    cy.get("dialog").should("not.exist");
  });

  it("closes with the browser's back button", () => {
    cy.visit("/");
    cy.holdGauge("Kitchen");

    cy.go("back");

    cy.get("dialog").should("not.exist");
    cy.location("hash").should("eq", "");
  });
});
