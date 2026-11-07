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

  it("shows the firmware version next to the name", () => {
    cy.stubApi(statusData({ version: "v1.2.0" }));
    cy.visit("/");

    cy.get("nav .version").should("have.text", "v1.2.0").parent().should("contain", "FritzGate");
  });

  it("shows no version for firmware that doesn't report it", () => {
    cy.stubApi(statusData());
    cy.visit("/");

    cy.contains("article", "Thermostats");
    cy.get("nav .version").should("not.exist");
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
      // Sensor gauges have no thermostat to configure, they show the sensor's history
      cy.get(".hold-button").should("have.length", 1).and("have.text", "Show the history of Sensor EEEE");
    });
  });

  it("shows a sensor that appears later, also in browsers without notifications", () => {
    // iOS Safari has no Notification outside home-screen apps, Android Chrome throws on `new Notification`
    let sensors = statusData().sensors;
    cy.stubApi(() => statusData({ sensors }));
    cy.visit("/", {
      onBeforeLoad: (win) => {
        delete (win as Partial<Window & typeof globalThis>).Notification;
      },
    });

    cy.contains("article", "Thermostats");
    cy.then(() => (sensors = [...sensors, unusedSensor]));
    cy.contains("article", "Sensors").contains("textPath", "Sensor EEEE");

    // The dashboard keeps updating afterwards
    cy.then(() => (sensors = sensors.map((v) => (v.id === unusedSensor.id ? { ...v, humidity: 77 } : v))));
    cy.contains("article", "Sensors").find(".slider-content").should("contain", "77%");
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
