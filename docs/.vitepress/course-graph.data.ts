import { createCourseGraphLoader } from './course-graph.mjs';

export interface CourseGraphNode {
  id: string;
  url: string;
  title: string;
  label: string;
  kind: 'lesson' | 'chapter' | 'note' | 'optional' | 'lab' | 'topic' | 'resource' | 'page';
}

export interface CourseGraph {
  nodes: CourseGraphNode[];
  links: { source: string; target: string }[];
}

export declare const data: CourseGraph;

export default createCourseGraphLoader();
