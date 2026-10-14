#!/bin/sh
DIR="$(cd "$(dirname "$0")" && pwd)"
JAR="$DIR/gradle/wrapper/gradle-wrapper.jar"
if [ -f "$JAR" ]; then
  exec java -cp "$JAR" org.gradle.wrapper.GradleWrapperMain "$@"
fi
# fallback: системный gradle
exec gradle "$@"
