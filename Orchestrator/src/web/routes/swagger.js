import swaggerJsdoc from 'swagger-jsdoc';
import swaggerUi from 'swagger-ui-express';

/**
 * Swagger/OpenAPI Setup
 */
export function setupSwagger(app, config) {
  const swaggerConfig = config.getSwaggerConfig();

  if (!swaggerConfig.enabled) {
    return;
  }

  const options = {
    definition: {
      openapi: '3.0.0',
      info: {
        title: 'UWB Test Orchestrator API',
        version: '1.0.0',
        description: 'REST API for UWB DWM3001C test rig orchestrator',
      },
      servers: [
        {
          url: `http://localhost:${config.getWebConfig().port}`,
          description: 'Development server',
        },
      ],
    },
    apis: ['./src/web/routes/*.js', './src/web/server.js'],
  };

  const swaggerSpec = swaggerJsdoc(options);

  app.use(swaggerConfig.path || '/api-docs', swaggerUi.serve);
  app.get(swaggerConfig.path || '/api-docs', swaggerUi.setup(swaggerSpec, {
    customCss: '.swagger-ui .topbar { display: none }',
  }));

  // JSON endpoint
  app.get('/api-docs.json', (req, res) => {
    res.setHeader('Content-Type', 'application/json');
    res.send(swaggerSpec);
  });
}

