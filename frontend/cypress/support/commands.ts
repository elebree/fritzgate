/// <reference types="cypress" />
import type { StatusData } from "../../src/interfaces";
import { corsHeaders } from "./api";

declare global {
  namespace Cypress {
    interface Chainable {
      /**
       * Stubs the whole FritzGate API. GET /api/status answers with `status`, or with what it
       * returns at request time when it is a function. The POSTs are aliased @binding and @fritzauth.
       */
      stubApi(status: StatusData | (() => StatusData)): Chainable<void>;
      /** The gauge of a thermostat on the dashboard */
      gauge(name: string): Chainable<JQuery<HTMLElement>>;
      /** Presses a thermostat gauge, as the start of a long press */
      pressGauge(name: string): Chainable<JQuery<HTMLElement>>;
      /** Releases a pressed thermostat gauge */
      releaseGauge(name: string): Chainable<JQuery<HTMLElement>>;
      /** Holds a thermostat gauge until its dialog opens, then releases it */
      holdGauge(name: string): Chainable<void>;
    }
  }
}

Cypress.Commands.add("stubApi", (status) => {
  // Anything not stubbed below fails, instead of reaching a real FritzGate on the network
  cy.intercept("**/api/**", { statusCode: 404, headers: corsHeaders });
  cy.intercept("OPTIONS", "**/api/**", { statusCode: 204, headers: corsHeaders });
  cy.intercept("GET", "**/api/status", (req) => {
    req.reply({
      headers: corsHeaders,
      body: typeof status === "function" ? status() : status,
    });
  }).as("status");
  cy.intercept("POST", "**/api/thermostat", { headers: corsHeaders, body: {} }).as("binding");
  cy.intercept("POST", "**/api/fritzauth", { headers: corsHeaders, body: {} }).as("fritzauth");
});

Cypress.Commands.add("gauge", (name) =>
  // Each holdable gauge contains a hidden "Configure <name>" button for keyboard users
  cy.contains(".hold-button", `Configure ${name}`).parent()
);

Cypress.Commands.add("pressGauge", (name) =>
  cy.gauge(name).trigger("pointerdown", {
    eventConstructor: "PointerEvent",
    button: 0,
    pointerType: "touch",
  })
);

Cypress.Commands.add("releaseGauge", (name) =>
  // The dialog may cover the gauge by now
  cy.gauge(name).trigger("pointerup", {
    eventConstructor: "PointerEvent",
    button: 0,
    pointerType: "touch",
    force: true,
  })
);

Cypress.Commands.add("holdGauge", (name) => {
  cy.pressGauge(name);
  cy.get("dialog[open]");
  cy.releaseGauge(name);
});

export {};
