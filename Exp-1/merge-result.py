import sys
import re


def escape(s : str):
  return f'"{s}"'

if __name__ == '__main__':
  res_dir = sys.argv[1]

  APPS = 'new_tcp_connections ssh_brute superspreader port_scan ddos syn_flood completed_flow slowloris_attack'.split()
  CONFIGS = ['TW1', 'TW2', 'OTW', 'ITW', 'OSW']

  with open(f'{res_dir}/precision.csv', 'w') as fp_precision:
    with open(f'{res_dir}/recall.csv', 'w') as fp_recall:
      print(' ,' + ','.join(map(escape, CONFIGS)), file=fp_precision)
      print(' ,' + ','.join(map(escape, CONFIGS)), file=fp_recall)
      for app in APPS:
        precisions = [app]
        recalls = [app]
        with open(f'{sys.argv[1]}/{app}.txt') as fp_result:
          result = fp_result.readlines()
        for conf in CONFIGS:
          try:
            i = result.index(f'{conf}\n')
            precisions.append(re.match(r'  Precision avg\. ([\d\.]*)$', result[i + 3])[1])
            recalls.append(re.match(r'  Recall avg\. ([\d\.]*)$', result[i + 4])[1])
          except ValueError:
            precisions.append('N/A')
            recalls.append('N/A')
        print(','.join(precisions), file=fp_precision)
        print(','.join(recalls), file=fp_recall)

