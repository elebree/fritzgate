import { celsius, corsHeaders, historyData, statusData, unusedSensor } from "../support/api";

describe("history", () => {
  beforeEach(() => {
    cy.stubApi(statusData());
  });

  it("shows the last 24 hours of the thermostat and its reference sensor", () => {
    cy.visit("/#thermostat-16");

    cy.wait("@history").its("request.query").should("deep.equal", { hours: "24", step: "300" });
    cy.get("dialog .history").within(() => {
      cy.contains("button", "24 h").should("have.attr", "aria-pressed", "true");
      cy.get(".readout")
        .should("contain", `${celsius(19.7)}℃ Govee H5075 DDDD`)
        .and("contain", `${celsius(18.5)}℃ Thermostat`)
        .and("contain", `${celsius(22)}℃ Target`)
        .and("contain", "41% Humidity")
        .and("contain", `+${celsius(1)}℃ Offset`);
      cy.get("figcaption").should("have.length", 3);
      cy.contains("figure", "Temperature").find("path.line").should("have.length", 3);
      cy.contains("figure", "Humidity").find("path.line").should("have.length", 1);
      cy.contains("figure", "Offset").find("path.line").should("have.length", 1);
    });
  });

  it("shows only the thermostat without a reference sensor", () => {
    cy.visit("/#thermostat-16");
    cy.get("dialog select").select("");

    cy.get("dialog .readout").should("not.contain", "Govee").and("contain", "Thermostat");
    cy.get("dialog figcaption").should("have.length", 2);
    cy.contains("dialog figure", "Humidity").should("not.exist");
  });

  it("shows the sensor chosen in the dialog before it is saved", () => {
    cy.visit("/#thermostat-16");
    cy.wait("@history");

    cy.get("dialog select").select("GVH5075_AAAA");

    cy.get("dialog .readout").should("contain", `${celsius(20.1)}℃ Govee H5075 AAAA`);
    cy.get("@history.all").should("have.length", 1);
    cy.get("@binding.all").should("have.length", 0);
  });

  it("loads another range", () => {
    cy.visit("/#thermostat-16");
    cy.wait("@history");

    cy.contains("dialog button", "7 days").click();

    cy.wait("@history").its("request.query").should("deep.equal", { hours: "168", step: "1800" });
    cy.contains("dialog button", "7 days").should("have.attr", "aria-pressed", "true");
    cy.contains("dialog button", "24 h").should("have.attr", "aria-pressed", "false");

    cy.contains("dialog button", "30 days").click();
    cy.wait("@history").its("request.query").should("deep.equal", { hours: "720", step: "7200" });
  });

  it("shows the values under the pointer, and the newest ones again when it leaves", () => {
    cy.visit("/#thermostat-16");
    cy.get("dialog .readout").should("contain", celsius(19.7));

    cy.contains("dialog figure", "Temperature")
      .find(".plot")
      .trigger("pointermove", "center", { eventConstructor: "PointerEvent" });
    cy.get("dialog .crosshair").should("have.length", 3);
    // FritzGate wasn't running yet in the first half of the day
    cy.get("dialog .readout").should("contain", "– Govee H5075 DDDD");

    cy.contains("dialog figure", "Temperature")
      .find(".plot")
      .trigger("pointerleave", { eventConstructor: "PointerEvent" });
    cy.get("dialog .crosshair").should("not.exist");
    cy.get("dialog .readout").should("contain", celsius(19.7));
  });

  it("goes through time with the arrow keys", () => {
    cy.visit("/#thermostat-16");

    cy.contains("dialog figure", "Temperature").find(".plot").focus().type("{leftarrow}");

    cy.get("dialog .readout").should("contain", `${celsius(19.2)}℃ Govee H5075 DDDD`);
    cy.contains("dialog figure", "Temperature")
      .find(".plot")
      .should("have.attr", "aria-valuenow", "286")
      .and("have.attr", "aria-valuetext")
      .and("contain", `Govee H5075 DDDD ${celsius(19.2)}℃`);

    cy.contains("dialog figure", "Temperature").find(".plot").type("{end}");
    cy.get("dialog .readout").should("contain", celsius(19.7));
  });

  it("shows the readings as a table, newest first", () => {
    cy.visit("/#thermostat-16");

    cy.contains("dialog summary", "Table").click();

    cy.get("dialog table thead th").should("have.length", 6);
    // only the points with readings, of the second half of the day
    cy.get("dialog table tbody tr").should("have.length", 144);
    cy.get("dialog table tbody tr")
      .first()
      .should("contain", celsius(19.7))
      .and("contain", "41%")
      .and("contain", `+${celsius(1)}℃`);
    cy.get("dialog table tbody tr").eq(1).should("contain", celsius(19.2));
  });

  it("shows heating that is off as target off", () => {
    const history = historyData();
    const kitchen = history.thermostats.find((thermostat) => thermostat.id === "16")!;
    kitchen.targetTemperature[kitchen.targetTemperature.length - 1] = 0;
    cy.intercept({ method: "GET", pathname: "/api/history" }, { headers: corsHeaders, body: history });

    cy.visit("/#thermostat-16");

    cy.get("dialog .readout").should("contain", "off Target");
  });

  it("explains a history without readings, over the empty charts", () => {
    const last = Math.floor(Date.now() / 1000 / 300) * 300;
    cy.intercept(
      { method: "GET", pathname: "/api/history" },
      { headers: corsHeaders, body: { last, step: 300, sensors: [], thermostats: [] } }
    );

    cy.visit("/#thermostat-16");

    cy.get("dialog .empty").should("contain", "No readings yet");
    cy.get("dialog figure").should("have.length", 3);
    cy.get("dialog .readout").should("contain", "– Thermostat");
  });

  it("keeps its height when the history arrives", () => {
    cy.intercept({ method: "GET", pathname: "/api/history" }, (req) => {
      req.reply({ delay: 1000, headers: corsHeaders, body: historyData() });
    });
    cy.visit("/#thermostat-16");

    // empty charts while the history loads
    cy.get("dialog .readout").should("contain", "– Thermostat");
    cy.get("dialog figure").should("have.length", 3);
    cy.get("dialog .history")
      .invoke("outerHeight")
      .then((loading) => {
        cy.get("dialog .readout").should("contain", `${celsius(18.5)}℃ Thermostat`);
        cy.get("dialog .history").invoke("outerHeight").should("eq", loading);
      });
  });

  it("keeps the dialog usable when the history fails", () => {
    cy.intercept({ method: "GET", pathname: "/api/history" }, { statusCode: 500, headers: corsHeaders });

    cy.visit("/#thermostat-16");

    cy.get("dialog").should("contain", "couldn't load the history");
    cy.get("dialog select").select("GVH5075_AAAA");
    cy.contains("dialog button", "Save").click();
    cy.wait("@binding");
  });

  it("refreshes every minute while the dialog is open", () => {
    cy.clock(Date.now(), ["setTimeout", "clearTimeout"]);
    cy.visit("/#thermostat-16");
    // the next refresh is scheduled once the answer is read
    cy.get("dialog .readout");

    cy.tick(60 * 1000);

    cy.wait("@history");
  });

  describe("of a sensor without thermostat", () => {
    beforeEach(() => {
      const data = statusData();
      data.sensors.push(unusedSensor);
      cy.stubApi(data);
    });

    it("opens after holding the sensor's gauge", () => {
      cy.visit("/");

      cy.contains(".hold-button", "Show the history of Sensor EEEE")
        .parent()
        .trigger("pointerdown", { eventConstructor: "PointerEvent", button: 0, pointerType: "touch" });

      cy.get("dialog[open]").should("contain", "Sensor EEEE");
      cy.location("hash").should("eq", "#sensor-GVH5075_EEEE");
    });

    it("shows the temperature and humidity of the sensor", () => {
      cy.visit("/");
      cy.contains(".hold-button", "Show the history of Sensor EEEE").click({ force: true });

      cy.wait("@history").its("request.query").should("deep.equal", { hours: "24", step: "300" });
      cy.get("dialog[open]").within(() => {
        cy.get("header").should("contain", "Sensor EEEE").and("contain", "GVH5075_EEEE");
        cy.get(".readout")
          .should("contain", `${celsius(20.1)}℃ Sensor EEEE`)
          .and("contain", "33% Humidity")
          .and("not.contain", "Thermostat")
          .and("not.contain", "Offset");
        cy.get("figcaption").should("have.length", 2);
        // the time axis goes under the humidity chart
        cy.contains("figure", "Humidity").find("text").should("contain", ":");
      });
    });

    it("shows only the temperature of a sensor without humidity", () => {
      const data = statusData();
      data.sensors.push({ ...unusedSensor, humidity: 0 });
      cy.stubApi(data);

      cy.visit("/#sensor-GVH5075_EEEE");

      cy.get("dialog figcaption").should("have.length", 1).and("contain", "Temperature");
      cy.get("dialog .readout").should("not.contain", "Humidity");
    });

    it("closes with the browser's back button", () => {
      cy.visit("/");
      cy.contains(".hold-button", "Show the history of Sensor EEEE").click({ force: true });
      cy.get("dialog[open]");

      cy.go("back");

      cy.get("dialog").should("not.exist");
      cy.location("hash").should("eq", "");
    });
  });
});
