import { celsius, statusData, unusedSensor } from "../support/api";

const columns = ($grid: JQuery<HTMLElement>) =>
  $grid[0].ownerDocument
    .defaultView!.getComputedStyle($grid[0])
    .gridTemplateColumns.split(" ").length;

describe("dashboard", () => {
  it("waits while the gateway doesn't answer", () => {
    cy.stubApi(statusData());
    cy.intercept("GET", "**/api/status", { forceNetworkError: true }).as("offline");
    cy.visit("/");

    cy.wait("@offline");
    cy.contains("trying to connect to the FritzGate");
  });

  it("waits while no sensors or thermostats are found", () => {
    cy.stubApi(statusData({ sensors: [], thermostats: [] }));
    cy.visit("/");

    cy.contains("trying hard to find sensors nearby");
  });

  it("shows a gauge for every thermostat", () => {
    cy.stubApi(statusData());
    cy.visit("/");

    cy.contains("article", "Thermostats").within(() => {
      cy.get(".gauges > *").should("have.length", 5);
      for (const name of ["Kitchen", "Living room", "Bedroom", "Child's room", "Bathroom"]) {
        cy.gauge(name).should("be.visible");
      }
    });
  });

  it("shows the reading of the reference sensor with the thermostat's own below", () => {
    cy.stubApi(statusData());
    cy.visit("/");

    // Kitchen measures 18.5 itself and uses Govee H5075 DDDD, which reads 19.7 and 41%
    cy.gauge("Kitchen").within(() => {
      cy.contains("textPath", "Govee H5075 DDDD");
      cy.get(".slider-content").should("contain", celsius(19.7)).and("contain", "41%");
      cy.get(".badge.bottom").should("contain", celsius(18.5));
    });
  });

  it("shows the built-in reading of a thermostat without reference sensor", () => {
    cy.stubApi(statusData());
    cy.visit("/");

    // Bathroom has no reference sensor in the fixture
    cy.gauge("Bathroom").within(() => {
      cy.contains("textPath", "built-in sensor");
      cy.get(".slider-content").should("contain", celsius(20.5)).and("not.contain", "%");
    });
  });

  it("marks a reference sensor that isn't seen", () => {
    const data = statusData();
    data.thermostats[4].referenceSensor = "FFFF";
    cy.stubApi(data);
    cy.visit("/");

    cy.gauge("Bathroom").contains("textPath", "FFFF (unavailable)");
  });

  it("shows every thermostat that shares a reference sensor", () => {
    const data = statusData();
    data.thermostats[4].referenceSensor = data.thermostats[0].referenceSensor;
    cy.stubApi(data);
    cy.visit("/");

    cy.gauge("Kitchen").contains("textPath", "Govee H5075 DDDD");
    cy.gauge("Bathroom").contains("textPath", "Govee H5075 DDDD");
  });

  it("lists only the sensors no thermostat uses", () => {
    const data = statusData();
    data.sensors.push(unusedSensor);
    cy.stubApi(data);
    cy.visit("/");

    cy.contains("article", "Sensors").within(() => {
      cy.get(".gauges > *").should("have.length", 1);
      cy.contains("textPath", "Sensor EEEE");
      // Sensor gauges have no thermostat to configure
      cy.get(".hold-button").should("not.exist");
    });
  });

  it("hides the sensors section when every sensor is in use", () => {
    cy.stubApi(statusData());
    cy.visit("/");

    cy.contains("article", "Thermostats");
    cy.contains("article header", "Sensors").should("not.exist");
  });

  it("fits two gauges per row on phones and more than four on wide screens", () => {
    cy.stubApi(statusData());

    cy.viewport(384, 832);
    cy.visit("/");
    cy.get(".gauges").first().should(($grid) => expect(columns($grid)).to.eq(2));

    cy.viewport(1920, 1080);
    cy.get(".gauges").first().should(($grid) => expect(columns($grid)).to.be.above(4));
  });
});
