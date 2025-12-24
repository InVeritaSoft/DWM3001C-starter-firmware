import { getConfig } from './config.js';
import { RS485Comm } from './rs485Comm.js';
import { NodeController } from './nodeController.js';
import { CSVLogger } from './csvLogger.js';
import { TestRunner } from './testRunner.js';

/**
 * Main Orchestrator Entry Point
 * Runs test plan from command line
 */
async function main() {
  console.log('UWB Test Orchestrator Starting...\n');

  try {
    // Load configuration
    const config = getConfig();
    config.loadSettings();
    config.loadTestPlan();

    const serialConfig = config.getSerialConfig();
    const testPlan = config.getTestPlan();

    if (testPlan.length === 0) {
      console.error('No tests found in test plan');
      process.exit(1);
    }

    // Initialize RS-485 connections
    console.log('Initializing RS-485 connections...');
    const rs485A = new RS485Comm(
      serialConfig.node_a_port,
      serialConfig.baudrate,
      serialConfig.timeout * 1000
    );
    const rs485B = new RS485Comm(
      serialConfig.node_b_port,
      serialConfig.baudrate,
      serialConfig.timeout * 1000
    );

    // Initialize node controllers
    const nodeA = new NodeController('A', rs485A);
    const nodeB = new NodeController('B', rs485B);

    // Initialize CSV logger
    const csvLogger = new CSVLogger();

    // Initialize test runner
    const testRunner = new TestRunner(nodeA, nodeB, csvLogger, config);

    // Connect to nodes in parallel for faster startup
    console.log('Connecting to nodes...');
    const [connectedA, connectedB] = await Promise.all([
      nodeA.connect().then(() => {
        console.log('Node A connected');
        return true;
      }),
      nodeB.connect().then(() => {
        console.log('Node B connected');
        return true;
      })
    ]);
    console.log('');

    // Test connectivity with PING
    console.log('Testing connectivity...');
    const pingA = await nodeA.ping();
    if (pingA) {
      console.log('✓ Node A PING successful');
    } else {
      console.error('✗ Node A PING failed - check connection');
    }

    const pingB = await nodeB.ping();
    if (pingB) {
      console.log('✓ Node B PING successful');
    } else {
      console.error('✗ Node B PING failed - check connection');
    }

    if (!pingA || !pingB) {
      console.error('\nError: One or both nodes are not responding to PING');
      console.error('Please check:');
      console.error('  1. Serial port connections');
      console.error('  2. RS-485 wiring (A+, B-, GND)');
      console.error('  3. Board power and firmware');
      process.exit(1);
    }
    console.log('');

    // Run test plan
    console.log(`Running test plan with ${testPlan.length} test(s)...\n`);
    await testRunner.runTestPlan(testPlan);

    console.log('\nAll tests completed!');

    // Disconnect
    await nodeA.disconnect();
    await nodeB.disconnect();
    testRunner.close();

    process.exit(0);
  } catch (error) {
    console.error('Fatal error:', error);
    process.exit(1);
  }
}

// Handle graceful shutdown
process.on('SIGTERM', () => {
  console.log('\nSIGTERM received, shutting down...');
  process.exit(0);
});

process.on('SIGINT', () => {
  console.log('\nSIGINT received, shutting down...');
  process.exit(0);
});

// Run main function
main();

