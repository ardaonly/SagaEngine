# SagaPipeline

SagaPipeline is the Saga-specific workflow orchestration layer.

It coordinates standalone tools for Saga product workflows without moving Saga
product knowledge into generic tools.

Ownership boundaries:



Tools/Forge
  generic build frontend and process orchestration
  must not own SagaEditor composition domain defaults

Tools/Prism
  generic code and artifact intelligence
  must not own Saga product pipeline policy



Tools/SagaTools
  thin dispatcher
  must not own orchestration logic
```

Current V1 command:

```txt
saga-pipeline editor-composition build --project .
```









