import { mount } from 'svelte';
import './app.scss';
import App from './App.svelte';
import { DemoApiService } from './services/demoApiService';
import { FritzGateApiService } from './services/fritzGateApiService';

const isDemo = window.location.hostname.endsWith('.github.io') || window.location.hostname === 'fritzgate.elebree.com';
const apiService = isDemo
  ? new DemoApiService()
  : new FritzGateApiService()
const app = mount(App, {
  target: document.getElementById('app')!,
  props: {
    api: apiService,
  }
});

export default app
